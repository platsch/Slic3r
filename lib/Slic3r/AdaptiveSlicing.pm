package Slic3r::AdaptiveSlicing;
use Moo;

use Slic3r::Geometry qw(X Y Z triangle_normal scale unscale);

# public
has 'mesh' => (is => 'ro', required => 1);
has 'size' => (is => 'ro', required => 1);

#private
has 'normal'  			=> (is => 'ro', default => sub { [] }); # facet_id => [normal];
has 'ordered_facets' 	=> (is => 'ro', default => sub { [] }); # id => [facet_id, min_z, max_z];
has 'current_facet' 	=> (is => 'rw');

sub BUILD {
    my $self = shift;
    
    my $facet_id = 0; 
    
    # generate facet normals
	foreach my $facet (@{$self->mesh->facets}) {
		my $normal = triangle_normal(map $self->mesh->vertices->[$_], @$facet[-3..-1]);
		# normalize length
		my $normal_length = sqrt($normal->[0]**2 + $normal->[1]**2 + $normal->[2]**2);
		$self->normal->[$facet_id] = [ map $normal->[$_]/$normal_length, (X,Y,Z) ];
		$facet_id++;
	}
	
	# generate a list of facet_ids, containing maximum and minimum Z-Value of the facet ordered by minimum Z
	my @sort_facets;
	for ($facet_id = 0; $facet_id <= $#{$self->mesh->facets}; $facet_id++) {
		my $max_a = $self->mesh->vertices->[$self->mesh->facets->[$facet_id]->[1]]->[Z];
		my $max_b = $self->mesh->vertices->[$self->mesh->facets->[$facet_id]->[2]]->[Z];
		push @sort_facets, [$facet_id, $self->mesh->vertices->[$self->mesh->facets->[$facet_id]->[0]]->[Z], ($max_a > $max_b) ? $max_a : $max_b];	
	}		
	@sort_facets = sort {$a->[1] <=> $b->[1]} @sort_facets;
	for (my $i = 0; $i <= $#sort_facets; $i++) {
		$self->ordered_facets->[$i] = $sort_facets[$i];
	} 
	# initialize pointer for cusp_height run
	$self->current_facet(0);
}

sub cusp_height {
	my $self = shift;
	my ($z, $cusp_value, $min_height, $max_height) = @_;
	
	my $height = $max_height;
	my $first_hit = 0;
	
	# find all facets intersecting the slice-layer
	my $ordered_id = $self->current_facet;
	while ($ordered_id <= $#{$self->ordered_facets}) {
		
		# facet's minimum is higher than slice_z -> end loop
		if($self->ordered_facets->[$ordered_id]->[1] > $z) {
			last;
		}
		
		# facet's maximum is higher than slice_z -> store the first event for next cusp_height call to begin at this point
		if($self->ordered_facets->[$ordered_id]->[2] > $z) {
			# first event?
			if(!$first_hit) {
				$first_hit = 1;
				$self->current_facet($ordered_id);
			} 
			
			# compute cusp-height for this facet and store minimum of all heights
			my $cusp = $self->_facet_cusp_height($ordered_id, $cusp_value);
			$height = $cusp if($cusp < $height);			
		}
		$ordered_id++;
	}
	# lower height limit due to printer capabilities
	$height = $min_height if($height < $min_height);
	
	# check for sloped facets inside the determined layer and correct height if necessary
	if($height > $min_height){
		while ($ordered_id <= $#{$self->ordered_facets}) {
			
			# facet's minimum is higher than slice_z + height -> end loop
			if($self->ordered_facets->[$ordered_id]->[1] > ($z + scale $height)) {
				last;
			}
			
			# Compute cusp-height for this facet and check against height.
			# Cusp height is computed for the lower surface of the this slice only, changes in geometry
			# straight above this plane are not taken into account for now.
			my $cusp = $self->_facet_cusp_height($ordered_id, $cusp_value);
			my $z_diff = unscale ($self->ordered_facets->[$ordered_id]->[1] - $z); 
			if( $cusp > $z_diff) {
				if($cusp < $height) {
					Slic3r::debugf "cusp computation, height is reduced from %f", $height;
					$height = $cusp;
					Slic3r::debugf "to %f due to new cusp height\n", $height;
				}
			}else{
				Slic3r::debugf "cusp computation, height is reduced from %f", $height;
				$height = $z_diff;
				Slic3r::debugf "to to z-diff: %f\n", $height;
			}
			
			$ordered_id++;	
		}
	}
	
	Slic3r::debugf "cusp computation, slize at z:%f, cusp_value:%f, resulting layer height:%f\n", unscale $z, $cusp_value, $height;
	
	return $height; 
}

sub _facet_cusp_height {
	my $self = shift;
	my ($ordered_id, $cusp_value) = @_;
	
	my $normal_z = $self->normal->[$self->ordered_facets->[$ordered_id]->[0]]->[Z];
	my $cusp = ($normal_z == 0) ? 9999 : abs($cusp_value/$normal_z);
	return $cusp;
}

1;