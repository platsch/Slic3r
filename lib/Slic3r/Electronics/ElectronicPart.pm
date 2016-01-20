package Slic3r::Electronics::ElectronicPart;
use strict;
use warnings;
use utf8;

use Slic3r::Electronics::Geometrics;
use Slic3r::Point;
use Slic3r::Polygon;
use Slic3r::ExPolygon;
use Slic3r::Geometry qw(X Y Z deg2rad convex_hull scale unscale);
use List::Util qw[min max];


use Slic3r::Geometry::Clipper qw(diff diff_ex);

#######################################################################
# Purpose    : Generates new Part
# Parameters : Name, library, deviceset, device and package of new part
# Returns    : Reference to new Part
# Commet     :
#######################################################################
sub new {
    my $class = shift;
    my $self = {};
    bless ($self, $class);
    my ($config,$name,$library,$deviceset,$device,$package) = @_;
    $self->{config} = $config;
    $self->{name} = $name;
    $self->{library} = $library;
    $self->{deviceset} = $deviceset;
    $self->{device} = $device;
    $self->{package} = $package;
    $self->{height} = undef;
    $self->{volume} = undef;
    $self->{chipVolume} = undef;
    $self->{shown} = 0;
    $self->{printed} = 0;
    
    my @position = @{$self->{position}} = (undef,undef,undef);
    my @rotation = @{$self->{rotation}} = (0,0,0);
    
    my @padlist = @{$self->{padlist}} = ();
    
    my @componentpos = @{$self->{componentpos}} = (0,0,0);
    
    my @componentsize = @{$self->{componentsize}} = (0,0,0);

    return $self;
}

#######################################################################
# Purpose    : Removes the position of the part
# Parameters : none
# Returns    : none
# Commet     : Doesnt removes the part itself
#######################################################################
sub removePart {
    my $self = shift;
    $self->{volume} = undef; 
    $self->{chipVolume} = undef; 
    @{$self->{position}} = (undef,undef,undef);
    @{$self->{rotation}} = (0,0,0);
}

#######################################################################
# Purpose    : Sets the position of the part
# Parameters : x, y, z coordinates of the part
# Returns    : none
# Commet     : coordinates have to be valid
#######################################################################
sub setPosition {
    my $self = shift;
    my ($x,$y,$z) = @_;
    $self->{position} = [$x,$y,$z];
}

#######################################################################
# Purpose    : Sets the rotation angles of the part
# Parameters : x, y, z rotation angles
# Returns    : none
# Commet     : rotation angles have to be in degree and valid
#######################################################################
sub setRotation {
    my $self = shift;
    my ($x,$y,$z) = @_;
    $self->{rotation} = [$x,$y,$z];
}

#######################################################################
# Purpose    : Sets the componentsize of the part itself
# Parameters : x, y, z dimensions of the part
# Returns    : none
# Commet     : values have to be valid
#######################################################################
sub setPartsize {
    my $self = shift;
    my ($x,$y,$z) = @_;
    $self->{componentsize} = [$x,$y,$z];
}

#######################################################################
# Purpose    : returns the componentsize of the part
# Parameters : none
# Returns    : (x, y, z) dimensions of the part
# Commet     : when the dimensions are not set,
#            : they are calculated by the footprint
#######################################################################
sub getPartsize {
    my $self = shift;
    if (!(defined($self->{componentsize}[0]) && defined($self->{componentsize}[1]) && defined($self->{componentsize}[2]))) {
        my $xmin = 0;
        my $ymin = 0;
        my $xmax = 0;
        my $ymax = 0;
        for my $pad (@{$self->{padlist}}) {
            if ($pad->{type} eq 'smd') {
                $xmin = min($xmin, $pad->{position}[0]-$pad->{size}[0]/2);
                $xmax = max($xmax, $pad->{position}[0]+$pad->{size}[0]/2);
                $ymin = min($ymin, $pad->{position}[1]-$pad->{size}[1]/2);
                $ymax = max($ymax, $pad->{position}[1]+$pad->{size}[1]/2);
            }
            if ($pad->{type} eq 'pad') {
                $xmin = min($xmin, $pad->{position}[0]-($pad->{drill}/2+0.25));
                $xmax = max($xmax, $pad->{position}[0]+($pad->{drill}/2+0.25));
                $ymin = min($ymin, $pad->{position}[1]-($pad->{drill}/2+0.25));
                $ymax = max($ymax, $pad->{position}[1]+($pad->{drill}/2+0.25));
            }
        }
        my $x = $xmax-$xmin;
        my $y = $ymax-$ymin;
        my $z = $self->getPartheight;
        @{$self->{componentsize}} = ($x,$y,$z);
        @{$self->{componentpos}} = (($xmax-abs($xmin))/2,($ymax-abs($ymin))/2,0);
    }
    
    return ($self->{componentsize}[0] + $self->{config}->{offset}{chip_x_offset},
    	$self->{componentsize}[1] + $self->{config}->{offset}{chip_y_offset},
    	$self->{componentsize}[2] + $self->{config}->{offset}{chip_z_offset});
}

#######################################################################
# Purpose    : returns the height of the chip
# Parameters : none
# Returns    : height of chip
# Commet     : 
#######################################################################
sub getPartheight {
    my $self = shift;
    if ($self->{config}->{chip_height}{$self->{package}}) {
        return $self->{config}->{chip_height}{$self->{package}};
    } else {
        return $self->{config}->{chip_height}{default};
    }
}

#######################################################################
# Purpose    : Sets the Partposition of the part itself
# Parameters : x, y, z coordinates of the part
# Returns    : none
# Commet     : values have to be valid
#######################################################################
sub setPartpos {
    my $self = shift;
    my ($x,$y,$z) = @_;
    $self->{componentpos} = [$x,$y,$z];
}

#######################################################################
# Purpose    : adds a pad to the footprint of the part
# Parameters : see Slic3r::Electronics::ElectronicPad->new
# Returns    : none
# Commet     :
#######################################################################
sub addPad {
    my $self = shift;
    my $pad = Slic3r::Electronics::ElectronicPad->new(@_);
    push @{$self->{padlist}}, $pad;
}

#######################################################################
# Purpose    : Gives a model of the parts footprint
# Parameters : none
# Returns    : Footprint model
# Commet     : The model is translated and rotated
#######################################################################
sub getFootprintModel {
    my $self = shift;
    my ($rot) = @_;
    my @triangles = ();
    for my $pad (@{$self->{padlist}}) {
        if ($pad->{type} eq 'smd') {
            push @triangles, Slic3r::Electronics::Geometrics->getCube(@{$pad->{position}}, ($pad->{size}[0], $pad->{size}[1], $self->{height}*(-1)), $pad->{rotation}[2]);
        }
        if ($pad->{type} eq 'pad') {
            push @triangles, Slic3r::Electronics::Geometrics->getCylinder(@{$pad->{position}}, $pad->{drill}/2+0.25, $self->{height}*(-1));
        }
    }
    my $model = $self->getTriangleMesh($rot, @triangles);
    return $model;
}

#######################################################################
# Purpose    : Gives a model of the parts
# Parameters : none
# Returns    : Part model
# Commet     : The model is translated and rotated
#######################################################################
sub getPartModel {
    my $self = shift;
    my ($rot) = @_;
    my @triangles = ();
    push @triangles, Slic3r::Electronics::Geometrics->getCube(@{$self->{componentpos}}, $self->getPartsize, 0);
    my $model = $self->getTriangleMesh($rot, @triangles);
    return $model;
}

#######################################################################
# Purpose    : Converts triagles to a triaglesMesh
# Parameters : Triagles to convert
# Returns    : a Model
# Commet     : Translates and rotates the model
#######################################################################
sub getTriangleMesh {
    my $self = shift;
    my ($rot, @triangles) = @_;
    my $vertices = $self->{vertices} = [];
    my $facets = $self->{facets} = [];
    for my $triangle (@triangles) {
        my @newTriangle = ();
        for my $point (@$triangle) {
            push @newTriangle, $self->getVertexID(@$point);
        }
        push @{$self->{facets}}, [@newTriangle];
    }
    
    my $mesh = Slic3r::TriangleMesh->new;
    $mesh->ReadFromPerl($self->{vertices}, $self->{facets});
    $mesh->repair;
    $mesh->rotate_x(deg2rad($self->{rotation}[0])) if ($self->{rotation}[0] != 0);
    $mesh->rotate_y(deg2rad($self->{rotation}[1])) if ($self->{rotation}[1] != 0);
    $mesh->rotate_z(deg2rad($self->{rotation}[2])+$rot);
    $mesh->translate($self->transformWorldtoObject($rot,(0,0,0)));
    
    
    my $model = Slic3r::Model->new;
    
    my $object = $model->add_object(name => $self->{name});
    my $volume = $object->add_volume(mesh => $mesh, name => $self->{name});
    
    return $model;
}

#######################################################################
# Purpose    : Returns the convex hull of the part, including pins and pads
#              to be removed from single layers during the slicing process
# Parameters : none
# Returns    : A polygon
# Comment    : How about rotations in X/Y?
#######################################################################
sub getHullPolygon {
	my $self = shift;
	my ($z) = @_;
	
	# part placed?
	if(!@{$self->{position}}[0]) {
		return undef
	}
	
	# part affected?
	if($z > $self->{position}[2] - $self->{height} && $z < ($self->{position}[2] + $self->{componentsize}[2])) {
		my @points;
		
		# outline of smd (and TH) pads
		for my $pad (@{$self->{padlist}}) {
	        if ($pad->{type} eq 'smd') {
	            my $dx = $pad->{size}[0]/2;
	            my $dy = $pad->{size}[1]/2;
	            push @points, Slic3r::Point->new(scale $pad->{position}[0]+$dx, scale $pad->{position}[1]+$dy);
	            push @points, Slic3r::Point->new(scale $pad->{position}[0]+$dx, scale $pad->{position}[1]-$dy);
	            push @points, Slic3r::Point->new(scale $pad->{position}[0]-$dx, scale $pad->{position}[1]-$dy);
	            push @points, Slic3r::Point->new(scale $pad->{position}[0]-$dx, scale $pad->{position}[1]+$dy);
	        }
	        #if ($pad->{type} eq 'pad') {
	            #push @triangles, Slic3r::Electronics::Geometrics->getCylinder(@{$pad->{position}}, $pad->{drill}/2+0.25, $self->{height}*(-1));
	        #}
		}

		# outline of smd body
		my $dx = $self->{componentsize}[0]/2;
		my $dy = $self->{componentsize}[1]/2;
	    push @points, Slic3r::Point->new(scale $self->{componentpos}[0]+$dx, scale $self->{componentpos}[1]+$dy);
	    push @points, Slic3r::Point->new(scale $self->{componentpos}[0]+$dx, scale $self->{componentpos}[1]-$dy);
	    push @points, Slic3r::Point->new(scale $self->{componentpos}[0]-$dx, scale $self->{componentpos}[1]-$dy);
	    push @points, Slic3r::Point->new(scale $self->{componentpos}[0]-$dx, scale $self->{componentpos}[1]+$dy);
		
		#my $p = Slic3r::Polygon->new_scale([0, 10], [0, 0], [10, 0], [10, 10], [0, 10]);
		my $polygon = convex_hull(\@points);
		
		# apply object rotation
		$polygon->rotate(deg2rad($self->{rotation}[2]), Slic3r::Point->new(0, 0));
				
		# apply object translation
		my @pos = $self->transformObjecttoWorld(0, (0, 0, 0));
		$polygon->translate(scale $pos[0], scale $pos[1]);
		
		if (0) {
			require "Slic3r/SVG.pm";
			Slic3r::SVG::output(
				"polygontest.svg",
				no_arrows   => 1,
				red_expolygons  => [Slic3r::ExPolygon->new($polygon)],
			);
		}
		
		return $polygon;
	}else{
		return undef;
	}
}

#######################################################################
# Purpose    : Gives a vertex id for a given vertex
# Parameters : The vertex
# Returns    : An id
# Commet     : If the vertex doesnt exists it will be created
#######################################################################
sub getVertexID {
    my $self = shift;
    my @vertex = @_;
    my $id = 0;
    while ($id < scalar @{$self->{vertices}}) {
        if ( ${$self->{vertices}}[$id][0] == $vertex[0] && ${$self->{vertices}}[$id][1] == $vertex[1] && ${$self->{vertices}}[$id][2] == $vertex[2]) {;
            return $id;
        }
        $id += 1;
    }
    push (@{$self->{vertices}}, [@vertex]);
    return $id;
}

#######################################################################
# Purpose    : Transforms world coodrdinates to object coordinates
# Parameters : world coordinates and rotation in rad
# Returns    : transformed coordinates
# Commet     : does not transform z axis
#######################################################################
sub transformWorldtoObject {
    my $self = shift;
    my ($rot,@trans) = @_;
    my @pos = ($self->{position}[0]-$trans[0],$self->{position}[1]-$trans[1],$self->{position}[2]);
    if ($rot != 0){
        @pos = ($pos[0]*cos($rot)+$pos[1]*sin($rot),$pos[0]*(-sin($rot))+$pos[1]*cos($rot),$pos[2]);
    }
    
    @pos = (int($pos[0]*1000)/1000.0,int($pos[1]*1000)/1000.0,$pos[2]);

    return @pos;
}

#######################################################################
# Purpose    : Transforms object coodrdinates to world coordinates
# Parameters : world coordinates and rotation in rad
# Returns    : transformed coordinates
# Commet     : does not transform z axis
#######################################################################
sub transformObjecttoWorld {
    my $self = shift;
    my ($rot,@trans) = @_;
    my @pos = ($self->{position}[0]+$trans[0],$self->{position}[1]+$trans[1],$self->{position}[2]);
    if ($rot != 0){
        @pos = ($pos[0]*cos($rot)+$pos[1]*sin($rot),$pos[0]*(-sin($rot))+$pos[1]*cos($rot),$pos[2]);
    }
    @pos = (int($pos[0]*1000)/1000.0,int($pos[1]*1000)/1000.0,$pos[2]);

    return @pos;
}

#######################################################################
# Purpose    : Returns the G-code for the placement
# Parameters : actual layer and $id of the part
# Returns    : G-code or ""
# Commet     : If part should not be placed now, return ""
#######################################################################
sub getPlaceGcode {
    my $self = shift;
    my ($printz, $id) = @_;
    my $gcode = "";
    if ($self->{printed} == 0 && defined($self->{position}[2]) && $self->getPlacementLayer <= $printz){
        $self->{printed} = 1;
        $gcode .= ";pick part nr " . $id . "\n";
        $gcode .= "M361 P" . $id . "\n";
    }
    return $gcode;
}

#######################################################################
# Purpose    : Returns the description for the placement
# Parameters : $id of the part
# Returns    : description 
# Commet     : 
#######################################################################
sub getPlaceDescription {
    my $self = shift;
    my ($id,@offset) = @_;
    my $gcode = "";
    if ($self->{printed}){
        my @newpos = $self->transformObjecttoWorld(0,@offset);
        $gcode .= ';<part id="' . $id . '" name="' . $self->{name} . '">' . "\n";
        $gcode .= ';  <position box="'.$id.'"/>' . "\n";
        $gcode .= ';  <size height="'.$self->{componentsize}[2].'"/>' . "\n";
        $gcode .= ';  <shape>' . "\n";
        $gcode .= ';    <point x="' . ($self->{componentpos}[0]-$self->{componentsize}[0]/2) . '" y="' . ($self->{componentpos}[1]-$self->{componentsize}[1]/2) . '"/>' . "\n";
        $gcode .= ';    <point x="' . ($self->{componentpos}[0]-$self->{componentsize}[0]/2) . '" y="' . ($self->{componentpos}[1]+$self->{componentsize}[1]/2) . '"/>' . "\n";
        $gcode .= ';    <point x="' . ($self->{componentpos}[0]+$self->{componentsize}[0]/2) . '" y="' . ($self->{componentpos}[1]+$self->{componentsize}[1]/2) . '"/>' . "\n";
        $gcode .= ';    <point x="' . ($self->{componentpos}[0]+$self->{componentsize}[0]/2) . '" y="' . ($self->{componentpos}[1]-$self->{componentsize}[1]/2) . '"/>' . "\n";
        $gcode .= ';  </shape>' . "\n";
        $gcode .= ';  <pads>' . "\n";
        for my $pad (@{$self->{padlist}}){
            $gcode .= ';    <pad x1="' . ($pad->{position}[0]-$pad->{size}[0]/2) . '" y1="' . ($pad->{position}[1]-$pad->{size}[1]/2) . '" x2="' . ($pad->{position}[0]+$pad->{size}[0]/2) . '" y2="' . ($pad->{position}[1]+$pad->{size}[1]/2) . '"/>' . "\n";
        }
        $gcode .= ';  </pads>' . "\n";
        $gcode .= ';  <destination x="' . $newpos[0] . '" y="' . $newpos[1] . '" z="' . $newpos[2] . '" orientation="' . $self->{rotation}[2] . '"/>' . "\n";
        $gcode .= ';</part>' . "\n";
        $gcode .= ';' . "\n";
    }
    return $gcode;
}

#######################################################################
# Purpose    : Returns the layer where the component is placed on
# Parameters : none
# Returns    : position 
# Commet     : 
#######################################################################
sub getPlacementLayer {
    my $self = shift;
    return $self->{position}[2]+$self->{componentsize}[2];
}

package Slic3r::Electronics::ElectronicPad;
use strict;
use warnings;
use utf8;

#######################################################################
# Purpose    : Creates a new pad
# Parameters : type, pin, pad, gate, x, y, r, dx, dy, drill, shape of the pad
# Returns    : A new Pad
# Commet     : 
#######################################################################
sub new {
    my $class = shift;
    my $self = {};
    bless ($self, $class);
    my ($type,$pad,$pin,$gate,$x,$y,$r,$dx,$dy,$drill,$shape) = @_;
    $self->{type} = $type;
    $self->{pad} = $pad;
    $self->{pin} = $pin;
    $self->{gate} = $gate;
    $self->{drill} = $drill;
    $self->{shape} = $shape;
    
    my @position = @{$self->{position}} = ($x,$y,0);
    my @size = @{$self->{size}} = ($dx,$dy,0);
    my @rotation = @{$self->{rotation}} = (0,0,$r);
    
    return $self
}

1;