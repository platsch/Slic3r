package Slic3r::Electronics::Filereaders::3DElectronics;
use strict;
use warnings;
use utf8;

use XML::LibXML;
use File::Basename;

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
    $file->addChild($dom->createAttribute( source => basename($schematic->getFilename)));
    
    # Export electronic components
    my $parts = $dom->createElement('parts');
    $root->addChild($parts);
    for my $part (@{$schematic->getPartlist}) {
        my $node = $dom->createElement('part');
        $parts->addChild($node);
        $node->addChild($dom->createAttribute( name => $part->getName));
        $node->addChild($dom->createAttribute( library => $part->getLibrary));
        $node->addChild($dom->createAttribute( deviceset => $part->getDeviceset));
        $node->addChild($dom->createAttribute( device => $part->getDevice));
        $node->addChild($dom->createAttribute( package => $part->getPackage));
        
        my $attributes = $dom->createElement('attributes');
        $node->addChild($attributes);
        $attributes->addChild($dom->createAttribute( partHeight => $part->getPartHeight));
        $attributes->addChild($dom->createAttribute( footprintHeight => $part->getFootprintHeight));
        $attributes->addChild($dom->createAttribute( placed => $part->isPlaced));
        
        my $pos = $dom->createElement('position');
        $node->addChild($pos);
        my $position = $part->getPosition;
        $pos->addChild($dom->createAttribute( X => $position->x));
        $pos->addChild($dom->createAttribute( Y => $position->y));
        $pos->addChild($dom->createAttribute( Z => $position->z));
        
        my $rot = $dom->createElement('rotation');
        $node->addChild($rot);
        my $rotation = $part->getRotation;
        $rot->addChild($dom->createAttribute( X => $rotation->x));
        $rot->addChild($dom->createAttribute( Y => $rotation->y));
        $rot->addChild($dom->createAttribute( Z => $rotation->z));
    }
    
#    # Export netpoints and connections
#    for my $net (@{$schematic->getPartlist}) {
#    
#    my $netpoints = $dom->createElement('netpoints');
#    $root->addChild($netpoints);
#    for my $part (@{$schematic->getPartlist}) {
#        my $node = $dom->createElement('part');
#        $parts->addChild($node);
#        $node->addChild($dom->createAttribute( name => $part->getName));
#        $node->addChild($dom->createAttribute( library => $part->getLibrary));
#        $node->addChild($dom->createAttribute( deviceset => $part->getDeviceset));
#        $node->addChild($dom->createAttribute( device => $part->getDevice));
#        $node->addChild($dom->createAttribute( package => $part->getPackage));
#        
#        my $attributes = $dom->createElement('attributes');
#        $node->addChild($attributes);
#        $attributes->addChild($dom->createAttribute( partHeight => $part->getPartHeight));
#        $attributes->addChild($dom->createAttribute( footprintHeight => $part->getFootprintHeight));
#        $attributes->addChild($dom->createAttribute( placed => $part->isPlaced));
#        
#        my $pos = $dom->createElement('position');
#        $node->addChild($pos);
#        my $position = $part->getPosition;
#        $pos->addChild($dom->createAttribute( X => $position->x));
#        $pos->addChild($dom->createAttribute( Y => $position->y));
#        $pos->addChild($dom->createAttribute( Z => $position->z));
#        
#        my $rot = $dom->createElement('rotation');
#        $node->addChild($rot);
#        my $rotation = $part->getRotation;
#        $rot->addChild($dom->createAttribute( X => $rotation->x));
#        $rot->addChild($dom->createAttribute( Y => $rotation->y));
#        $rot->addChild($dom->createAttribute( Z => $rotation->z));
#    }
#    
    my ($base,$path,$type) = fileparse($schematic->getFilename,('.sch','.SCH','3de','.3DE'));
    my $newpath = $path . $base . ".3de";
    return $dom->toFile($newpath, 2);
}

1;