package Slic3r::ElectronicPart;
use strict;
use warnings;

use parent qw(Exporter);

our @EXPORT_OK = qw(PM_AUTOMATIC PM_MANUAL PM_NONE);
our %EXPORT_TAGS = (PlacingMethods => \@EXPORT_OK);

1;
