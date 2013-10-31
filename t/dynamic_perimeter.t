use Test::More tests => 4;
use strict;
use warnings;

BEGIN {
    use FindBin;
    use lib "$FindBin::Bin/../lib";
}

use List::Util qw(first);
use Slic3r;
use Slic3r::Test qw(_eq);
use Slic3r::Geometry qw(Z PI scale unscale);

my $config = Slic3r::Config->new_from_defaults;


# generates gcode from the cube_with_hole testmodel.
# gcode gets parsed at layer $z ($print_z), the number of perimeters and spacing between perimeter lines
# is checked against given values for the vertical perimeter lines at the left side of the cube.
my $test = sub {
    my ($conf, $z, $perimeters, $width) = @_;
    $conf ||= $config;
    
    my $print = Slic3r::Test::init_print('cube_with_hole', config => $conf);
    
    my @x_values = [];
    Slic3r::GCode::Reader->new->parse(Slic3r::Test::gcode($print), sub {
        my ($self, $cmd, $args, $info) = @_;
        
        if($info->{new_Z}) {
	        if (($info->{new_Z} == $z) && ($info->{dist_X} == 0) && ($info->{dist_Y} > 5) && ($info->{extruding})) {
	        	if($args->{X}) {
	        		push @x_values, $args->{X};
	        	}
	        }
        }
    });
    
	@x_values = sort {$a <=> $b} @x_values;
	
	for (my $i = 1; $i < $perimeters; $i++) {
		#is ($x_values[$i]-$x_values[$i-1], $width, 'correct perimeter width in gcode');
		ok  (_eq($x_values[$i]-$x_values[$i-1], $width), 'correct perimeter width in gcode, loop: ' . $i);
	}
	is ($#x_values/2, $perimeters, 'correct number of perimeters');
	
	1;
};
        

$config->set('start_gcode', '');  # to avoid dealing with the nozzle lift in start G-code
$config->set('z_offset', 0);
$config->set('adaptive_slicing', 1);
$config->set('extrusion_width', 0.5);
$config->set('first_layer_height', 0.4);
$config->set('nozzle_diameter', [0.5]);
$config->set('min_layer_height', [0.1]);
$config->set('max_layer_height', [0.4]);


subtest 'spacing->width computation' => sub {
	plan tests => 48;
	for (my $nozzle = 0.1; $nozzle < 0.8; $nozzle+=0.2) { # 4 iterations
		for (my $layer = 0.1; $layer < 0.6; $layer+=0.2) { # 3 iterations
			for (my $spacing = 0.2; $spacing < 0.9; $spacing+=0.2) { # 4 iterations
				
				my $flow = Slic3r::Flow->new(
	            	nozzle_diameter => $nozzle,
	            	layer_height => $layer
	        	);
	
				$flow->width_from_spacing($spacing);
				is ($flow->spacing, $spacing, 'correct spacing->width->spacing');
			}
		}
	}
};

# standard case without width-reduction
my $p = 3;
$config->set('perimeters', $p);
subtest 'gcode line distance - no reduction' => sub {
	plan tests => $p;
	$test->($config, 0.800, $p, 0.5);
};
	
# standard case with width-reduction
$p = 6;
$config->set('perimeters', $p);
subtest 'gcode line distance - reduction preserving nr of perimeters' => sub {
	plan tests => $p;
	$test->($config, 0.800, $p, 0.4160);
};

#$Slic3r::debug = 1;
# width-reduction and decreasing number of perimeters
$p = 8;
$config->set('perimeters', $p);
subtest 'gcode line distance - reduction preserving nr of perimeters' => sub {
	plan tests => $p-1;
	$test->($config, 0.800, $p-1, 0.355);
};	
	
__END__
