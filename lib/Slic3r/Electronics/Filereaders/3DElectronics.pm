package Slic3r::Electronics::Filereaders::3DElectronics;
use strict;
use warnings;
use utf8;

use XML::LibXML;
use File::Basename;
use Slic3r::Electronics::ElectronicPart;

#######################################################################
# Purpose    : Reads file of the 3DElectronics type
# Parameters : Filename to read
# Returns    : Schematic of electronics
# Commet     : Calls read file of source file
#######################################################################
sub readFile {
    my $self = shift;
    my ($filename,$schematic, $config) = @_;

    my $parser = XML::LibXML->new();
    my $xmldoc = $parser->parse_file($filename);
    my ($base,$path,$type) = fileparse($filename,('.sch','.SCH','3de','.3DE'));
    $schematic->{partlist} = ();
    for my $files ($xmldoc->findnodes('/electronics/filename')) {
        $schematic->{filename} = $files->getAttribute('source');
        Slic3r::Electronics::Electronics->readFile($path . $schematic->{filename},$schematic, $config);
    }
    for my $node ($xmldoc->findnodes('/electronics/parts/part')) {
        my $name = $node->getAttribute('name');
        my $library = $node->getAttribute('library');
        my $deviceset = $node->getAttribute('deviceset');
        my $device = $node->getAttribute('device');
        my $package = $node->getAttribute('package');
        my $height = undef;
        my @position = undef;
        my @rotation = undef;
        my @componentpos = undef;
        my @componentsize = undef;
        for my $h ($node->findnodes('./attributes')) {
            $height = $h->getAttribute('height');
        }
        for my $pos ($node->findnodes('./position')) {
            @position = ($pos->getAttribute('X'),$pos->getAttribute('Y'),$pos->getAttribute('Z'));
        }
        for my $rot ($node->findnodes('./rotation')) {
            @rotation = ($rot->getAttribute('X'),$rot->getAttribute('Y'),$rot->getAttribute('Z'));
        }
        for my $chip ($node->findnodes('./componentsize')) {
            @componentsize = ($chip->getAttribute('X'),$chip->getAttribute('Y'),$chip->getAttribute('Z'));
        }
        for my $componentpos ($node->findnodes('./componentpos')) {
            @componentpos = ($componentpos->getAttribute('X'),$componentpos->getAttribute('Y'),$componentpos->getAttribute('Z'));
        }
        for my $part (@{$schematic->{partlist}}) {
            if (($part->{name} eq $name) && 
                    ($part->{library} eq $library) && 
                    ($part->{deviceset} eq $deviceset) && 
                    ($part->{device} eq $device) && 
                    ($part->{package} eq $package)) {
                $part->{height} = $height;
                @{$part->{position}} = @position;
                @{$part->{rotation}} = @rotation;
                @{$part->{componentsize}} = @componentsize;
                @{$part->{componentpos}} = @componentpos;
            }
        }
    }
    return $schematic;

}

#######################################################################
# Purpose    : Writes file of the 3DElectronics type
# Parameters : Schematic of electronics
# Returns    : boolean is save was  successful
# Commet     : 
#######################################################################
sub writeFile {
    my $self = shift;
    my ($schematic) = @_;
    my $dom = XML::LibXML::Document->createDocument('1.0','utf-8');
    my $root = $dom->createElement('electronics');
    $root->addChild($dom->createAttribute( version => '1.1'));
    $dom->setDocumentElement($root);

    my $file = $dom->createElement('filename');
    $root->addChild($file);
    $file->addChild($dom->createAttribute( source => basename($schematic->{filename})));
    
    my $parts = $dom->createElement('parts');
    $root->addChild($parts);
    for my $part (@{$schematic->{partlist}}) {
        if (defined($part->{position}[0]) && defined($part->{position}[1]) && defined($part->{position}[2])){
            my $node = $dom->createElement('part');
            $parts->addChild($node);
            $node->addChild($dom->createAttribute( name => $part->{name}));
            $node->addChild($dom->createAttribute( library => $part->{library}));
            $node->addChild($dom->createAttribute( deviceset => $part->{deviceset}));
            $node->addChild($dom->createAttribute( device => $part->{device}));
            $node->addChild($dom->createAttribute( package => $part->{package}));
            
            my $height = $dom->createElement('attributes');
            $node->addChild($height);
            $height->addChild($dom->createAttribute( height => $part->{height}));
            
            my $pos = $dom->createElement('position');
            $node->addChild($pos);
            $pos->addChild($dom->createAttribute( X => $part->{position}[0]));
            $pos->addChild($dom->createAttribute( Y => $part->{position}[1]));
            $pos->addChild($dom->createAttribute( Z => $part->{position}[2]));
            
            my $rot = $dom->createElement('rotation');
            $node->addChild($rot);
            $rot->addChild($dom->createAttribute( X => $part->{rotation}[0]));
            $rot->addChild($dom->createAttribute( Y => $part->{rotation}[1]));
            $rot->addChild($dom->createAttribute( Z => $part->{rotation}[2]));
            
            my $chip = $dom->createElement('componentsize');
            $node->addChild($chip);
            $chip->addChild($dom->createAttribute( X => $part->{componentsize}[0]));
            $chip->addChild($dom->createAttribute( Y => $part->{componentsize}[1]));
            $chip->addChild($dom->createAttribute( Z => $part->{componentsize}[2]));
            
            my $componentpos = $dom->createElement('componentpos');
            $node->addChild($componentpos);
            $componentpos->addChild($dom->createAttribute( X => $part->{componentpos}[0]));
            $componentpos->addChild($dom->createAttribute( Y => $part->{componentpos}[1]));
            $componentpos->addChild($dom->createAttribute( Z => $part->{componentpos}[2]));
        }
    }
    my ($base,$path,$type) = fileparse($schematic->{filename},('.sch','.SCH','3de','.3DE'));
    my $newpath = $path . $base . ".3de";
    return $dom->toFile($newpath, 0);
}

1;