package Slic3r::ElectronicPart;
use strict;
use warnings;

use parent qw(Exporter);

our @EXPORT_OK = qw(PM_AUTOMATIC PM_MANUAL PM_NONE CM_NONE CM_LAYER CM_PART);
our %EXPORT_TAGS = (PlacingMethods => [qw(PM_AUTOMATIC PM_MANUAL PM_NONE)], ConnectionMethods => [qw(CM_NONE CM_LAYER CM_PART)]);

1;