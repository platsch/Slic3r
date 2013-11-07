package Slic3r::Print::Region;
use Moo;

has 'extruders'         => (is => 'rw', default => sub { {} }); # by role
has 'flows'             => (is => 'rw', default => sub { {} }); # by role
has 'first_layer_flows' => (is => 'rw', default => sub { {} }); # by role


# Returns a new Region object. The member variables are pointing to 
# the existing (global) extruder- and flow objects
sub clone {
	my $self = shift;
    
    my $region = (ref $self)->new();
    for (qw(perimeter perimeter_2 infill solid_infill top_infill)) {
    	$region->extruders->{$_} = $self->extruders->{$_};
    	$region->flows->{$_} = $self->flows->{$_};
    	$region->first_layer_flows->{$_} = $self->first_layer_flows->{$_};
    }
    
    return $region;
}

1;
