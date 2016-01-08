package Slic3r::Electronics::ElectronicNet;
use strict;
use warnings;
use utf8;

#######################################################################
# Purpose    : Creates a new net
# Parameters : name
# Returns    : A new Net
# Commet     : 
#######################################################################
sub new {
    my $class = shift;
    my $self = {};
    bless ($self, $class);
    my ($name) = @_;
    $self->{name} = $name;
    my @pinlist = @{$self->{pinlist}} = ();
    
    return $self
}

#######################################################################
# Purpose    : Adds a new Pin to the Net
# Parameters : see Slic3r::Electronics::ElectronicNetPin
# Returns    : none
# Commet     : 
#######################################################################
sub addPin {
    my $self = shift;
    my $pad = Slic3r::Electronics::ElectronicNetPin->new(@_);
    push @{$self->{pinlist}}, $pad;
}


package Slic3r::Electronics::ElectronicNetPin;
use strict;
use warnings;
use utf8;

#######################################################################
# Purpose    : Creates a new NetPin
# Parameters : part, gate, pin
# Returns    : A new NetPin
# Commet     : 
#######################################################################
sub new {
    my $class = shift;
    my $self = {};
    bless ($self, $class);
    my ($part,$gate,$pin) = @_;
    $self->{part} = $part;
    $self->{gate} = $gate;
    $self->{pin} = $pin;
    
    return $self
}

1;