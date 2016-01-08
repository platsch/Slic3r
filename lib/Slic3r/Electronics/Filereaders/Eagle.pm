package Slic3r::Electronics::Filereaders::Eagle;
use strict;
use warnings;
use utf8;

use XML::LibXML;
use Slic3r::Electronics::ElectronicPart;
use Slic3r::Electronics::ElectronicNet;
use List::Util qw[min max];

#######################################################################
# Purpose    : Reads file of the Eagle type
# Parameters : Filename to read
# Returns    : Schematic of electronics
# Commet     : 
#######################################################################
sub readFile {
    my $self = shift;
    my ($filename,$schematic, $config) = @_;

    my $parser = XML::LibXML->new();
    my $xmldoc = $parser->parse_file($filename);
    
    $schematic->{filename} = $filename;

    for my $sheet ($xmldoc->findnodes('/eagle/drawing/schematic/sheets/sheet')) {
        for my $instance ($sheet->findnodes('./instances/instance')) {
            my $part = $instance->getAttribute('part');
            for my $partlist ($xmldoc->findnodes("/eagle/drawing/schematic/parts/part[\@name='$part']")) {
                my $library = $partlist->getAttribute('library');
                my $deviceset = $partlist->getAttribute('deviceset');
                my $device = $partlist->getAttribute('device');
                for my $devicesetlist ($xmldoc->findnodes("/eagle/drawing/schematic/libraries/library[\@name='$library']/devicesets/deviceset[\@name='$deviceset']/devices/device[\@name='$device']")) {
                    my $package = $devicesetlist->getAttribute('package');
                    if (defined $package) {
                        my $newpart = Slic3r::Electronics::ElectronicPart->new(
                            $part,
                            $library,
                            $deviceset,
                            $device,
                            $package,
                        );
                        for my $packagelist ($xmldoc->findnodes("/eagle/drawing/schematic/libraries/library[\@name='$library']/packages/package[\@name='$package']/smd")) {
                            my $shape = $packagelist->getAttribute('shape');
                            my $rotation = $packagelist->getAttribute('rot');
                            if (! (defined $shape)) {
                                $shape = 'none';
                            }
                            if (defined $rotation) {
                                $rotation =~ s/R//;
                            } else {
                                $rotation = 0;
                            }
                            my $pad = $packagelist->getAttribute('name');
                            for my $pinlist ($xmldoc->findnodes("/eagle/drawing/schematic/libraries/library[\@name='$library']/devicesets/deviceset[\@name='$deviceset']/devices/device[\@name='$device']/connects/connect[\@pad='$pad']")) {
                                $newpart->addPad(
                                    $packagelist->getName,
                                    $pad,
                                    $pinlist->getAttribute('pin'),
                                    $pinlist->getAttribute('gate'),
                                    $packagelist->getAttribute('x'),
                                    $packagelist->getAttribute('y'),
                                    $rotation,
                                    $packagelist->getAttribute('dx'),
                                    $packagelist->getAttribute('dy'),
                                    0,
                                    $shape,
                                );
                            }
                        }
                        for my $packagelist ($xmldoc->findnodes("/eagle/drawing/schematic/libraries/library[\@name='$library']/packages/package[\@name='$package']/pad")) {
                            my $shape = $packagelist->getAttribute('shape');
                            my $rotation = $packagelist->getAttribute('rot');
                            if (! (defined $shape)) {
                                $shape = 'none';
                            }
                            if (defined $rotation) {
                                $rotation =~ s/R//;
                            } else {
                                $rotation = 0;
                            }
                            my $pad = $packagelist->getAttribute('name');
                            for my $pinlist ($xmldoc->findnodes("/eagle/drawing/schematic/libraries/library[\@name='$library']/devicesets/deviceset[\@name='$deviceset']/devices/device[\@name='$device']/connects/connect[\@pad='$pad']")) {
                                $newpart->addPad(
                                    $packagelist->getName,
                                    $pad,
                                    $pinlist->getAttribute('pin'),
                                    $pinlist->getAttribute('gate'),
                                    $packagelist->getAttribute('x'),
                                    $packagelist->getAttribute('y'),
                                    $rotation,
                                    0,
                                    0,
                                    $packagelist->getAttribute('drill'),
                                    $shape,
                                );
                            }
                        }
                        my $xmin = 0;
                        my $ymin = 0;
                        my $xmax = 0;
                        my $ymax = 0;
                        for my $packagelist ($xmldoc->findnodes("/eagle/drawing/schematic/libraries/library[\@name='$library']/packages/package[\@name='$package']/wire")) {
                            if (21 == $packagelist->getAttribute('layer') || 51 == $packagelist->getAttribute('layer')){
                                my $x1 = $packagelist->getAttribute('x1');
                                my $x2 = $packagelist->getAttribute('x2');
                                my $y1 = $packagelist->getAttribute('y1');
                                my $y2 = $packagelist->getAttribute('y2');
                                $xmin = min($xmin, $x1, $x2);
                                $xmax = max($xmax, $x1, $x2);
                                $ymin = min($ymin, $y1, $y2);
                                $ymax = max($ymax, $y1, $y2);
                            }
                        }
                        for my $packagelist ($xmldoc->findnodes("/eagle/drawing/schematic/libraries/library[\@name='$library']/packages/package[\@name='$package']/rectangle")) {
                            if (21 == $packagelist->getAttribute('layer') || 51 == $packagelist->getAttribute('layer')){
                                my $x1 = $packagelist->getAttribute('x1');
                                my $x2 = $packagelist->getAttribute('x2');
                                my $y1 = $packagelist->getAttribute('y1');
                                my $y2 = $packagelist->getAttribute('y2');
                                $xmin = min($xmin, $x1, $x2);
                                $xmax = max($xmax, $x1, $x2);
                                $ymin = min($ymin, $y1, $y2);
                                $ymax = max($ymax, $y1, $y2);
                            }
                        }
                        for my $packagelist ($xmldoc->findnodes("/eagle/drawing/schematic/libraries/library[\@name='$library']/packages/package[\@name='$package']/circle")) {
                            if (21 == $packagelist->getAttribute('layer') || 51 == $packagelist->getAttribute('layer')){
                                my $x = $packagelist->getAttribute('x');
                                my $y = $packagelist->getAttribute('y');
                                my $r = $packagelist->getAttribute('radius');
                                $xmin = min($xmin, $x-$r);
                                $xmax = max($xmax, $x+$r);
                                $ymin = min($ymin, $y-$r);
                                $ymax = max($ymax, $y+$r);
                            }
                        }
                        my $x = $xmax-$xmin;
                        my $y = $ymax-$ymin;
                        my $z = $newpart->getPartheight($config);
                        $newpart->setPartsize($x+$config->{offset}{chip_x_offset},$y+$config->{offset}{chip_y_offset},$z+$config->{offset}{chip_z_offset});
                        $newpart->setPartpos($xmax-$x/2,$ymax-$y/2,0);
                        push @{$schematic->{partlist}}, $newpart;
                    } else {
                        print $part, " has no package assigned.\n";
                    }
                }
            }
        }
        for my $net ($sheet->findnodes('./nets/net')) {
            my $newnet = Slic3r::Electronics::ElectronicNet->new($net->getAttribute('name'));
            for my $segment ($net->findnodes('./segment')) {
                for my $pinref ($segment->findnodes('./pinref')) {
                    $newnet->addPin(
                        $pinref->getAttribute('part'),
                        $pinref->getAttribute('gate'),
                        $pinref->getAttribute('pin'),
                    );
                }
            }
            push @{$schematic->{netlist}}, $newnet;
        }
    }
    
    return $schematic;
}

1;