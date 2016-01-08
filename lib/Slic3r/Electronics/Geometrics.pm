package Slic3r::Electronics::Geometrics;
use strict;
use warnings;
use utf8;

use Slic3r::Geometry qw(deg2rad);

#######################################################################
# Purpose    : Creates a cube out of triangles
# Parameters : x, y, z position and dx, dy, dz dimensions
# Returns    : A cube
# Commet     :
#######################################################################
sub getCube {
    my $self = shift;
    my ($x,$y,$z,$dx,$dy,$dz,$rot) = @_;
    if ($rot == 90 || $rot == 270) {
        my $tmp = $dx;
        $dx = $dy;
        $dy = $tmp;
    }
    $x -= $dx/2;
    $y -= $dy/2;
    my @triangles = ();
    push @triangles, [[$x, $y, $z], [$x, $y+$dy, $z], [$x+$dx, $y+$dy, $z]]; #bottom
    push @triangles, [[$x, $y, $z], [$x+$dx, $y, $z], [$x+$dx, $y+$dy, $z]]; #bottom
    push @triangles, [[$x, $y, $z], [$x, $y+$dy, $z], [$x, $y+$dy, $z+$dz]]; #left
    push @triangles, [[$x, $y, $z], [$x, $y, $z+$dz], [$x, $y+$dy, $z+$dz]]; #left
    push @triangles, [[$x, $y+$dy, $z], [$x+$dx, $y+$dy, $z], [$x+$dx, $y+$dy, $z+$dz]]; #back
    push @triangles, [[$x, $y+$dy, $z], [$x, $y+$dy, $z+$dz], [$x+$dx, $y+$dy, $z+$dz]]; #back
    push @triangles, [[$x+$dx, $y, $z], [$x+$dx, $y+$dy, $z], [$x+$dx, $y+$dy, $z+$dz]]; #right
    push @triangles, [[$x+$dx, $y, $z], [$x+$dx, $y, $z+$dz], [$x+$dx, $y+$dy, $z+$dz]]; #right
    push @triangles, [[$x, $y, $z], [$x+$dx, $y, $z], [$x+$dx, $y, $z+$dz]]; #front
    push @triangles, [[$x, $y, $z], [$x, $y, $z+$dz], [$x+$dx, $y, $z+$dz]]; #front 
    push @triangles, [[$x, $y, $z+$dz], [$x, $y+$dy, $z+$dz], [$x+$dx, $y+$dy, $z+$dz]]; #top
    push @triangles, [[$x, $y, $z+$dz], [$x+$dx, $y, $z+$dz], [$x+$dx, $y+$dy, $z+$dz]]; #top
    
    return @triangles;
    
}

#######################################################################
# Purpose    : Creates a cylinder out of triangles
# Parameters : x, y, z position, radius and height
# Returns    : A Cylinder
# Commet     :
#######################################################################
sub getCylinder {
    my $self = shift;
    my ($x,$y,$z,$r,$h) = @_;
    my @triangles = ();
    my $steps = 16;
    my $stepsize = deg2rad(360/$steps);
    for my $i (1..$steps) {
        push @triangles, [ [$x, $y, $z], [$x+$r*cos(($i-1)*$stepsize), $y+$r*sin(($i-1)*$stepsize), $z], [$x+$r*cos($i*$stepsize), $y+$r*sin($i*$stepsize), $z] ]; #lower part
        push @triangles, [ [$x+$r*cos(($i-1)*$stepsize), $y+$r*sin(($i-1)*$stepsize), $z], [$x+$r*cos($i*$stepsize), $y+$r*sin($i*$stepsize), $z], [$x+$r*cos($i*$stepsize), $y+$r*sin($i*$stepsize), $z+$h] ]; #outer part
        push @triangles, [ [$x+$r*cos(($i-1)*$stepsize), $y+$r*sin(($i-1)*$stepsize), $z], [$x+$r*cos(($i-1)*$stepsize), $y+$r*sin(($i-1)*$stepsize), $z+$h], [$x+$r*cos($i*$stepsize), $y+$r*sin($i*$stepsize), $z+$h] ]; #outer part
        push @triangles, [ [$x, $y, $z+$h], [$x+$r*cos(($i-1)*$stepsize), $y+$r*sin(($i-1)*$stepsize), $z+$h], [$x+$r*cos($i*$stepsize), $y+$r*sin($i*$stepsize), $z+$h] ]; #upper part
    }
    return @triangles;
}

1;