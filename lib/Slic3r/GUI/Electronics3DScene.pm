package Slic3r::GUI::Electronics3DScene;
use strict;
use warnings;
use utf8;

use OpenGL qw(:glconstants :glfunctions :glufunctions :gluconstants);
use Wx::Event qw(EVT_MOUSE_EVENTS);
use base qw(Slic3r::GUI::3DScene);

use Data::Dumper;

#######################################################################
# Purpose    : Creates a new 3D scene
# Parameters : see Slic3r::GUI::3DScene
# Returns    : see Slic3r::GUI::3DScene
# Comment     :
#######################################################################
sub new {
    my ($class, $parent) = @_;
    my $self = $class->SUPER::new($parent);
    bless ($self, $class);
    $self->{parent} = $parent;
    
    EVT_MOUSE_EVENTS($self, \&mouse_event_new);
    
    return $self;
}

sub load_scene_volume {
	my ($self) = @_;
}

# Inteded to draw lines for rubberbanding vizualisation
sub draw_line {
	my ($self, $a, $b, $width, $color) = @_;
	
	# default color is black
	if(!defined $color) {
		$color = [0, 0, 0, 0.8];
	}
	
	my $tverts = Slic3r::GUI::_3DScene::GLVertexArray->new;	
	
	my $dx = $b->x-$a->x;
    my $dy = $b->y-$a->y;
    my $vector_l = sqrt($dx**2+$dy**2);
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

    push @{$self->volumes}, Slic3r::GUI::3DScene::Volume->new(
        bounding_box    => $bb,
        color           => $color,
        tverts			=> $tverts,
    );
}

sub load_electronic_part {
	my ($self, $mesh, $color) = @_;
	
	# default color is blue
	if(!defined $color) {
		$color = [0, 0, 0.9, 1.0];
	}
	
	my $tverts = Slic3r::GUI::_3DScene::GLVertexArray->new;
	$tverts->load_mesh($mesh);

    push @{$self->volumes}, Slic3r::GUI::3DScene::Volume->new(
        bounding_box    => $mesh->bounding_box,
        color           => $color,
        tverts			=> $tverts,
    );
}

#######################################################################
# Purpose    : Processes mouse events
# Parameters : An mouse event
# Returns    : none
# Comment     : Overloads the method mouse_event_new of Slic3r::GUI::3DScene
#######################################################################
sub mouse_event_new {
    my ($self, $e) = @_;
    if ($e->LeftUp && $self->{parent}->get_place) {
        my $cur_pos = $self->mouse_ray($e->GetX, $e->GetY)->intersect_plane($self->{parent}->get_z);
        my $item = $self->{parent}->get_place;
        if ($item->{type} eq 'part') {
            $self->{parent}->placePart($item->{part}, @$cur_pos);
        }
        if ($item->{type} eq 'volume' && $item->{volume}) {
            my $volume = $item->{volume};
            $self->{parent}->placePart($self->{parent}->findPartByVolume($volume), @$cur_pos);
        }        
        $self->{parent}->set_place(0);
    }
    else {
        $self->mouse_event($e);
    }
}

1;