package Slic3r::Layer;
use Moo;

use List::Util qw(first);
use Slic3r::Geometry qw(scale);
use Slic3r::Geometry::Clipper qw(union_ex);

has 'id'                => (is => 'rw', required => 1, trigger => 1); # sequential number of layer, 0-based
has 'object'            => (is => 'ro', weak_ref => 1, required => 1);
has 'regions'           => (is => 'ro', default => sub { [] });
has 'slicing_errors'    => (is => 'rw');

has 'slice_z'           => (is => 'ro', required => 1); # Z used for slicing in scaled coordinates
has 'print_z'           => (is => 'ro', required => 1); # Z used for printing in unscaled coordinates
has 'height'            => (is => 'ro', required => 1); # layer height in unscaled coordinates

# collection of expolygons generated by slicing the original geometry;
# also known as 'islands' (all regions and surface types are merged here)
has 'slices'            => (is => 'rw');

sub _trigger_id {
    my $self = shift;
    $_->_trigger_layer for @{$self->regions || []};
}

sub upper_layer_slices {
    my $self = shift;
    
    my $upper_layer = $self->object->layers->[ $self->id + 1 ] or return [];
    return $upper_layer->slices;
}

sub region {
    my $self = shift;
    my ($region_id) = @_;
    
    for (my $i = @{$self->regions}; $i <= $region_id; $i++) {
        $self->regions->[$i] //= Slic3r::Layer::Region->new(
            layer   => $self,
            region  => $self->object->print->regions->[$i],
        );
    }
    
    return $self->regions->[$region_id];
}

# merge all regions' slices to get islands
sub make_slices {
    my $self = shift;
    $self->slices(union_ex([ map $_->p, map @{$_->slices}, @{$self->regions} ]));
}

sub make_perimeters {
    my $self = shift;
    Slic3r::debugf "Making perimeters for layer %d\n", $self->id;
    $_->make_perimeters for @{$self->regions};
}

sub support_islands_enclose_line {
    my $self = shift;
    my ($line) = @_;
    return 0 if !$self->support_islands;   # why can we arrive here if there are no support islands?
    return (first { $_->encloses_line($line) } @{$self->support_islands}) ? 1 : 0;
}

package Slic3r::Layer::Support;
use Moo;
extends 'Slic3r::Layer';

# ordered collection of extrusion paths to fill surfaces for support material
has 'support_islands'           => (is => 'rw');
has 'support_fills'             => (is => 'rw');
has 'support_interface_fills'   => (is => 'rw');

1;
