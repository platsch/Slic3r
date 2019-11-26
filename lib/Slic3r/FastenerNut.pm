package Slic3r::FastenerNut;
use strict;
use warnings;

use parent qw(Exporter);

our @EXPORT_OK = qw(PM_AUTOMATIC PM_MANUAL PM_NONE PO_FLAT PO_UPRIGHT);
our %EXPORT_TAGS = (PlacingMethods => [qw(PM_AUTOMATIC PM_MANUAL PM_NONE)], PartOrientations => [qw(PO_FLAT PO_UPRIGHT)]);

1;