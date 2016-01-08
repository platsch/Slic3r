package Slic3r::GUI::Electronics3DScene;
use strict;
use warnings;
use utf8;

use Wx::Event qw(EVT_MOUSE_EVENTS);
use base qw(Slic3r::GUI::3DScene);

use Data::Dumper;

#######################################################################
# Purpose    : Creates a new 3D scene
# Parameters : see Slic3r::GUI::3DScene
# Returns    : see Slic3r::GUI::3DScene
# Commet     :
#######################################################################
sub new {
    my ($class, $parent) = @_;
    my $self = $class->SUPER::new($parent);
    bless ($self, $class);
    $self->{parent} = $parent;
    
    EVT_MOUSE_EVENTS($self, \&mouse_event_new);
    
    return $self;
}

#######################################################################
# Purpose    : Processes mouse events
# Parameters : An mouse event
# Returns    : none
# Commet     : Overloads the method mouse_event_new of Slic3r::GUI::3DScene
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