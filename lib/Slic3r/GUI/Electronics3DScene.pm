package Slic3r::GUI::Electronics3DScene;
use strict;
use warnings;
use utf8;

use OpenGL qw(:glconstants :glfunctions :glufunctions :gluconstants);
use List::Util qw(min max);
use Wx::Event qw(EVT_MOUSE_EVENTS);
use base qw(Slic3r::GUI::3DScene);

__PACKAGE__->mk_accessors( qw(on_place_part on_rubberband_split on_waypoint_split on_right_double_click) );

use Data::Dumper;

#######################################################################
# Purpose    : Creates a new 3D scene
# Parameters : see Slic3r::GUI::3DScene
# Returns    : see Slic3r::GUI::3DScene
# Comment     :
#######################################################################
sub new {
    my ($class, $parent, $schematic) = @_;
    my $self = $class->SUPER::new($parent);
    bless ($self, $class);
    $self->{parent} = $parent;
    
    $self->{schematic} = $schematic;
    $self->{current_z} = 0;
    $self->{visibility_offset} = 1; # Indicates how far electronic parts should be visible above the extrusion object
    
    $self->{activity}->{rubberband_splitting} = undef;
    
    EVT_MOUSE_EVENTS($self, \&mouse_event_new);

    return $self;
}

# Inteded to draw lines for rubberbanding vizualisation
# Points a and b are relative to the center of object
sub add_rubberband {
	my ($self, $a, $b, $width, $color) = @_;
	
	$a = $a->clone;
	$b = $b->clone;
	
	# default color is black
	if(!defined $color) {
		$color = [0, 0, 0, 0.8];
	}
	
	# default width
	if(!defined $width) {
		$width = 0.3;
	}
	
	my $tverts = Slic3r::GUI::_3DScene::GLVertexArray->new;	
	
	my $dx = $b->x-$a->x;
    my $dy = $b->y-$a->y;
    my $vector_l = max(0.001, sqrt($dx**2+$dy**2)); # lower limit to avoid division by zero during normalization
    my $x_off = ($dy*$width/2)/$vector_l;
    my $y_off = ($dx*$width/2)/$vector_l;
    
    # Translate points to origin
    $a->translate($self->origin->x, $self->origin->y, 0);
    $b->translate($self->origin->x, $self->origin->y, 0);
    
	$tverts->push_vert($a->x+$x_off, $a->y-$y_off, $a->z);
	$tverts->push_norm(0, 0, 1);
	$tverts->push_vert($b->x+$x_off, $b->y-$y_off, $b->z);
	$tverts->push_norm(0, 0, 1);
	$tverts->push_vert($a->x-$x_off, $a->y+$y_off, $a->z);
	$tverts->push_norm(0, 0, 1);
	
	$tverts->push_vert($a->x-$x_off, $a->y+$y_off, $a->z);
	$tverts->push_norm(0, 0, 1);
	$tverts->push_vert($b->x+$x_off, $b->y-$y_off, $b->z);
	$tverts->push_norm(0, 0, 1);
	$tverts->push_vert($b->x-$x_off, $b->y+$y_off, $b->z);
	$tverts->push_norm(0, 0, 1);

    my $bb = Slic3r::Geometry::BoundingBoxf3->new;
    $bb->merge_point($a);
    $bb->merge_point($b);

	my %offsets = (); # [ z => [ qverts_idx, tverts_idx ] ]    
    $offsets{0} = [0, 0];
    my $h = min($a->z, $b->z);
    $offsets{$h-$self->{visibility_offset}} = [0, 0];

    push @{$self->volumes}, Slic3r::GUI::3DScene::Volume->new(
        bounding_box    => $bb,
        color           => $color,
        tverts			=> $tverts,
        offsets         => { %offsets },
    );
    
    return $#{$self->volumes};
}

sub load_electronic_part {
	my ($self, $mesh, $color) = @_;
	
	# default color is blue
	if(!defined $color) {
		$color = [0, 0, 0.9, 1.0];
	}
	
	my $tverts = Slic3r::GUI::_3DScene::GLVertexArray->new;
	$tverts->load_mesh($mesh);

	my %offsets = (); # [ z => [ qverts_idx, tverts_idx ] ]    
    $offsets{0} = [0, 0];
    $offsets{$mesh->bounding_box->z_min-$self->{visibility_offset}} = [0, 0];
    print "load_electronic_part\n";

    push @{$self->volumes}, Slic3r::GUI::3DScene::Volume->new(
        bounding_box    => $mesh->bounding_box,
        color           => $color,
        tverts			=> $tverts,
        offsets         => { %offsets },
    );
    
    return $#{$self->volumes};
}

# start placing an electronic part.
# on_place_part will be called when the users clicks to place this object.
sub place_electronic_part {
	my ($self, $part) = @_;
	
	my $mesh = $part->getMesh;
	$mesh->translate($self->origin->x, $self->origin->y, $self->{current_z});
	$self->load_electronic_part($mesh, [0, 0, 0.7, 0.7]);
	
    $self->{activity}->{place_electronic_part} = $part;
    $self->enable_picking(0); # disable picking to preserve color (avoid hovering)
}

sub add_wire_point {
	my ($self, $point, $color) = @_;
	
	my $radius = 0.7;
	
	# default color is red
	if(!defined $color) {
		$color = [0.9, 0, 0, 0.9];
	}
	
	my $tverts = Slic3r::GUI::_3DScene::GLVertexArray->new;	
    
    # Translate points to origin
    $point = $point->clone;
    $point->translate($self->origin->x, $self->origin->y, 0);
    
    # generate polyhedron as simple sphere. Should be replaced by a true sphere...
    # upper part
	$tverts->push_vert($point->x, $point->y, $point->z+$radius);
	$tverts->push_vert($point->x+$radius, $point->y+$radius, $point->z);
	$tverts->push_vert($point->x-$radius, $point->y+$radius, $point->z);
	
	$tverts->push_vert($point->x, $point->y, $point->z+$radius);
	$tverts->push_vert($point->x+$radius, $point->y-$radius, $point->z);
	$tverts->push_vert($point->x+$radius, $point->y+$radius, $point->z);
	
	$tverts->push_vert($point->x, $point->y, $point->z+$radius);
	$tverts->push_vert($point->x-$radius, $point->y+$radius, $point->z);
	$tverts->push_vert($point->x-$radius, $point->y-$radius, $point->z);
	
	$tverts->push_vert($point->x, $point->y, $point->z+$radius);
	$tverts->push_vert($point->x-$radius, $point->y-$radius, $point->z);
	$tverts->push_vert($point->x+$radius, $point->y-$radius, $point->z);
	
	# lower part
	$tverts->push_vert($point->x, $point->y, $point->z-$radius);
	$tverts->push_vert($point->x-$radius, $point->y+$radius, $point->z);
	$tverts->push_vert($point->x+$radius, $point->y+$radius, $point->z);
	
	$tverts->push_vert($point->x, $point->y, $point->z-$radius);
	$tverts->push_vert($point->x+$radius, $point->y+$radius, $point->z);
	$tverts->push_vert($point->x+$radius, $point->y-$radius, $point->z);
	
	$tverts->push_vert($point->x, $point->y, $point->z-$radius);
	$tverts->push_vert($point->x-$radius, $point->y-$radius, $point->z);
	$tverts->push_vert($point->x-$radius, $point->y+$radius, $point->z);
	
	$tverts->push_vert($point->x, $point->y, $point->z-$radius);
	$tverts->push_vert($point->x+$radius, $point->y-$radius, $point->z);
	$tverts->push_vert($point->x-$radius, $point->y-$radius, $point->z);
	
	foreach my $i (0..23) {
		$tverts->push_norm(0, 0, 1);
	}
	
	my $bb = Slic3r::Geometry::BoundingBoxf3->new;
    $bb->merge_point($point);
    
    my %offsets = (); # [ z => [ qverts_idx, tverts_idx ] ]    
    $offsets{0} = [0, 0];
    $offsets{$point->z-$self->{visibility_offset}} = [0, 0];

    push @{$self->volumes}, Slic3r::GUI::3DScene::Volume->new(
        bounding_box    => $bb,
        color           => $color,
        tverts			=> $tverts,
        offsets         => { %offsets },
    );
    
    return $#{$self->volumes};
}

# starts a selection of a 3rd point with lines rendered to the endpoint of the rubberband
sub rubberband_splitting {
	my ($self, $rubberband) = @_;
	
	# if this rubberband is wired, simply split. Else: create a wire from the closest point.
	if($rubberband->isWired) {
		# add lines
		$self->add_rubberband($rubberband->a, $self->get_mouse_pos_3d_obj);
		$self->add_rubberband($rubberband->b, $self->get_mouse_pos_3d_obj);	
	}else{
		if($rubberband->pointASelected) {
			$self->add_rubberband($rubberband->a, $self->get_mouse_pos_3d_obj, 0.6);
			$self->add_rubberband($rubberband->b, $self->get_mouse_pos_3d_obj, 0.1);
		}else{
			$self->add_rubberband($rubberband->a, $self->get_mouse_pos_3d_obj, 0.6);
			$self->add_rubberband($rubberband->b, $self->get_mouse_pos_3d_obj, 0.1);
		}	
	}
	
	# set activity until next mouse click
	$self->{activity}->{rubberband_splitting} = $rubberband;
}

# starts a selection of a 2nd point with lines rendered to the endpoint of one rubberband
sub point_splitting {
	my ($self, $netPoint) = @_;
	
	$self->add_rubberband($netPoint->getPoint, $self->get_mouse_pos_3d_obj, 0.6);
	
	# set activity until next mouse click
	$self->{activity}->{point_splitting} = $netPoint;
}

# set current z height, must be updated every time the slider is moved!
sub set_z {
	my ($self, $z) = @_;
	$self->{current_z} = $z;
	
	# update parent z info
	$self->set_toolpaths_range(0, $z);
}

sub cancel_action {
	my ($self) = @_;
	
	if($self->{activity}->{rubberband_splitting}) {
		# remove current lines
        pop @{$self->volumes};
        pop @{$self->volumes};
        
        $self->{activity}->{rubberband_splitting} = undef;
	}
	if($self->{activity}->{point_splitting}) {
		# remove current lines
        pop @{$self->volumes};
        
        $self->{activity}->{point_splitting} = undef;
	}
	if($self->{activity}->{place_electronic_part}) {
		# remove part preview
        pop @{$self->volumes};
        
        $self->{activity}->{place_electronic_part} = undef;
        $self->enable_picking(1);
	}
	$self->Refresh;
}

#######################################################################
# Purpose    : Processes mouse events
# Parameters : A mouse event
# Returns    : none
# Comment     : Overloads the method mouse_event_new of Slic3r::GUI::3DScene
#######################################################################
sub mouse_event_new {
    my ($self, $e) = @_;
    if ($e->LeftUp && $self->{activity}->{place_electronic_part}) {
        $self->on_place_part->($self->{activity}->{place_electronic_part}, $self->get_mouse_pos_3d_obj)
        	if $self->on_place_part;
        	
        $self->{activity}->{place_electronic_part} = undef;
        $self->enable_picking(1);
    }	
    elsif ($e->Moving && $self->{activity}->{place_electronic_part}) {
        # refresh mouse position
        $self->mouse_event($e);
        my $mpos = $self->get_mouse_pos_3d_obj;

        # remove mesh
        pop @{$self->volumes};
        
        # generate mesh at new position. Not very efficient implementation...
        my $mesh = $self->{activity}->{place_electronic_part}->getMesh;
        $mesh->translate($self->origin->x + $mpos->x, $self->origin->y + $mpos->y, $self->{current_z});
        $self->load_electronic_part($mesh, [0, 0, 0.7, 0.7]);
        $self->volumes->[$#{$self->volumes}]->hover(0);
        #$self->volumes->[$#{$self->volumes}]->selected

        # refresh canvas
        $self->Refresh;
    }	
    elsif ($e->Moving && $self->{activity}->{rubberband_splitting}) {
    	# refresh mouse position
    	$self->mouse_event($e);
    	
    	# remove old lines
        pop @{$self->volumes};
        pop @{$self->volumes};
        
        my $rubberband = $self->{activity}->{rubberband_splitting};
        
        if($rubberband->isWired) {
        	# add new lines
	        $self->add_rubberband($self->{activity}->{rubberband_splitting}->a, $self->get_mouse_pos_3d_obj);
			$self->add_rubberband($self->{activity}->{rubberband_splitting}->b, $self->get_mouse_pos_3d_obj);
        }else{
        	my $source_wayPoint = $rubberband->pointASelected ? $rubberband->getNetPointA : $rubberband->getNetPointB;
        	my $to_netPoint = $self->{schematic}->findNearestSplittingPoint($source_wayPoint, $self->get_mouse_pos_3d_obj);
        	my $to_point = $to_netPoint ? $to_netPoint->getPoint : $self->get_mouse_pos_3d_obj;
	        if($rubberband->pointASelected) {
				$self->add_rubberband($rubberband->a, $to_point, 0.6);
				$self->add_rubberband($rubberband->b, $to_point, 0.1);
			}else{
				$self->add_rubberband($rubberband->a, $to_point, 0.1);
				$self->add_rubberband($rubberband->b, $to_point, 0.6);
			}	
        }
        
        # refresh canvas
        $self->Refresh;
    }
    elsif ($e->Moving && $self->{activity}->{point_splitting}) {
    	# refresh mouse position
    	$self->mouse_event($e);
    	
    	# remove old line
        pop @{$self->volumes};
        
        my $netPoint = $self->{activity}->{point_splitting};
        my $to_netPoint = $self->{schematic}->findNearestSplittingPoint($netPoint, $self->get_mouse_pos_3d_obj);
       	my $to_point = $to_netPoint ? $to_netPoint->getPoint : $self->get_mouse_pos_3d_obj;
        
	    $self->add_rubberband($self->{activity}->{point_splitting}->getPoint, $to_point, 0.6);
        
        # refresh canvas
        $self->Refresh;
    }
    elsif ($e->LeftDown && $self->{activity}->{rubberband_splitting}) {
    	# callback
    	$self->on_rubberband_split->($self->{activity}->{rubberband_splitting}, $self->get_mouse_pos_3d_obj)
            if $self->on_rubberband_split;
        
        $self->{activity}->{rubberband_splitting} = undef;
    }
    elsif ($e->LeftDown && $self->{activity}->{point_splitting}) {
    	# callback
    	$self->on_waypoint_split->($self->{activity}->{point_splitting}, $self->get_mouse_pos_3d_obj)
            if $self->on_waypoint_split;
        
        $self->{activity}->{point_splitting} = undef;
    }
    elsif($e->RightDClick) {
    	my $volume_idx = $self->_hover_volume_idx // -1;
    	$self->on_right_double_click->($volume_idx)
    		if $self->on_right_double_click;
    }
    else{
        $self->mouse_event($e);
    }
}

# Get 3D mouse position in scene coordinates
sub get_mouse_pos_3d {
	my $self = shift;
	return $self->mouse_ray($self->_mouse_pos->x, $self->_mouse_pos->y)->intersect_plane($self->{current_z});
}

# Get 3D mouse position in object coordinates
sub get_mouse_pos_3d_obj {
	my $self = shift;
	my $mouseP = $self->get_mouse_pos_3d;
    $mouseP->translate(-$self->origin->x, -$self->origin->y, 0);
	return $mouseP;
}

1;