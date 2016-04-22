package Slic3r::GUI::Plater::ObjectElectronicsDialog;
use strict;
use warnings;
use utf8;

use Wx qw(:dialog :id :misc :sizer :systemsettings :notebook wxTAB_TRAVERSAL);
use Wx::Event qw(EVT_BUTTON);
use base 'Wx::Frame';

#######################################################################
# Purpose    : Creates a new Frame for 3DElectronics
# Parameters : name, object model, schematic and source filename
# Returns    : A new Frame
# Comment     :
#######################################################################
sub new {
    my $class = shift;
    my ($parent, $print, %params) = @_;
    my $self = $class->SUPER::new($parent, -1, "3D Electronics for " . $params{object}->name, wxDefaultPosition, [800,600], &Wx::wxMAXIMIZE | &Wx::wxDEFAULT_FRAME_STYLE);
    $self->{$_} = $params{$_} for keys %params;
        
    $self->{tabpanel} = Wx::Notebook->new($self, -1, wxDefaultPosition, wxDefaultSize, wxNB_TOP | wxTAB_TRAVERSAL);
    $self->{tabpanel}->AddPage($self->{parts} = Slic3r::GUI::Plater::ElectronicsPanel->new(
    	$self->{tabpanel},
    	$parent,
    	$print,
    	obj_idx => $params{obj_idx},
    	model_object => $params{model_object},
    	schematic => $params{schematic}),
    "Electronics");

    my $sizer = Wx::BoxSizer->new(wxVERTICAL);
    $sizer->Add($self->{tabpanel}, 1, wxEXPAND | wxTOP | wxLEFT | wxRIGHT, 10);
    
    $self->SetSizer($sizer);
    $self->SetMinSize($self->GetSize);
    
    return $self;
}

sub reload_print{
	my $self = shift;
	$self->{parts}->reload_print;
}


package Slic3r::GUI::Plater::ElectronicsPanel;
use strict;
use warnings;
use utf8;

use Slic3r::Print::State ':steps';
use Slic3r::Electronics::Electronics;
use Slic3r::GUI::Electronics3DScene;
use Slic3r::Config;
use File::Basename qw(basename);
use Wx qw(:misc :sizer :slider :treectrl :button :filedialog wxTAB_TRAVERSAL wxSUNKEN_BORDER wxBITMAP_TYPE_PNG wxFD_OPEN wxFD_FILE_MUST_EXIST wxID_OK
    wxTheApp wxTE_READONLY);
use Wx::Event qw(EVT_BUTTON EVT_TREE_ITEM_COLLAPSING EVT_TREE_SEL_CHANGED EVT_SLIDER EVT_MOUSE_EVENTS);
use base qw(Wx::Panel Class::Accessor);
use Scalar::Util qw(blessed);
use File::Basename;
use Data::Dumper;

__PACKAGE__->mk_accessors(qw(print enabled _loaded canvas slider));

use constant ICON_OBJECT        => 0;
use constant ICON_SOLIDMESH     => 1;
use constant ICON_MODIFIERMESH  => 2;
use constant ICON_PCB           => 3;

#######################################################################
# Purpose    : Creates a Panel for 3DElectronics
# Parameters : model_object, schematic and source filename to edit
# Returns    : A Panel
# Comment     : Main Panel for 3D Electronics
#######################################################################
sub new {
    my $class = shift;
    my ($parent, $plater, $print, %params) = @_;
    my $self = $class->SUPER::new($parent, -1, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    $self->{plater} = $plater;
    $self->{obj_idx} = $params{obj_idx};
    my $object = $self->{model_object} = $params{model_object};
    $self->{schematic} = $params{schematic};
    my $place = $self->{place} = 0;
    $self->{model_object}->update_bounding_box;
    $self->{rootOffset} = $self->{model_object}->_bounding_box->center;
    my $configfile ||= Slic3r::decode_path(Wx::StandardPaths::Get->GetUserDataDir . "/electronics/electronics.ini");
    my $config = $self->{config};
    if (-f $configfile) {
        $self->{config} = eval { Slic3r::Config->read_ini($configfile) };
    } else {
        $self->createDefaultConfig($configfile);
    }
    # Lookup-table to match selected object ID from canvas to actual object
    $self->{object_list} = ();
    
    # upper buttons
    my $btn_load_netlist = $self->{btn_load_netlist} = Wx::Button->new($self, -1, "Load netlist", wxDefaultPosition, wxDefaultSize, wxBU_LEFT);
    
    # upper buttons sizer
    my $buttons_sizer = Wx::FlexGridSizer->new( 1, 3, 5, 5);
    $buttons_sizer->Add($btn_load_netlist, 0);
    $btn_load_netlist->SetFont($Slic3r::GUI::small_font);
    

    # create TreeCtrl
    my $tree = $self->{tree} = Wx::TreeCtrl->new($self, -1, wxDefaultPosition, [350,-1], 
        wxTR_NO_BUTTONS | wxSUNKEN_BORDER | wxTR_HAS_VARIABLE_ROW_HEIGHT
        | wxTR_SINGLE | wxTR_NO_BUTTONS | wxEXPAND);
    {
        $self->{tree_icons} = Wx::ImageList->new(16, 16, 1);
        $tree->AssignImageList($self->{tree_icons});
        $self->{tree_icons}->Add(Wx::Bitmap->new("$Slic3r::var/brick.png", wxBITMAP_TYPE_PNG));     # ICON_OBJECT
        $self->{tree_icons}->Add(Wx::Bitmap->new("$Slic3r::var/package.png", wxBITMAP_TYPE_PNG));   # ICON_SOLIDMESH
        $self->{tree_icons}->Add(Wx::Bitmap->new("$Slic3r::var/plugin.png", wxBITMAP_TYPE_PNG));    # ICON_MODIFIERMESH
        $self->{tree_icons}->Add(Wx::Bitmap->new("$Slic3r::var/PCB-icon.png", wxBITMAP_TYPE_PNG));  # ICON_PCB
        
        my $rootId = $tree->AddRoot("Object", ICON_OBJECT);
        $tree->SetPlData($rootId, { type => 'object' });
    }
    
    # mid buttons
    my $btn_place_part = $self->{btn_place_part} = Wx::Button->new($self, -1, "Place part", wxDefaultPosition, wxDefaultSize, wxBU_LEFT);
    my $btn_remove_part = $self->{btn_remove_part} = Wx::Button->new($self, -1, "Remove Part", wxDefaultPosition, wxDefaultSize, wxBU_LEFT);
    
    # mid buttons sizer
    my $buttons_sizer_mid = Wx::FlexGridSizer->new( 1, 3, 5, 5);
    $buttons_sizer_mid->Add($btn_place_part, 0);
    $buttons_sizer_mid->Add($btn_remove_part, 0);
    $btn_place_part->SetFont($Slic3r::GUI::small_font);
    $btn_remove_part->SetFont($Slic3r::GUI::small_font);
    
    # part settings fields
    my $name_text = $self->{name_text} = Wx::StaticText->new($self, -1, "Name:",wxDefaultPosition,[105,-1]);
    my $name_field = $self->{name_field} = Wx::TextCtrl->new($self, -1, "",wxDefaultPosition, [230,-1], wxTE_READONLY);
    
    my $library_text = $self->{library_text} = Wx::StaticText->new($self, -1, "Library:",wxDefaultPosition,[105,-1]);
    my $library_field = $self->{library_field} = Wx::TextCtrl->new($self, -1, "",wxDefaultPosition,  [230,-1], wxTE_READONLY);
    
    my $deviceset_text = $self->{deviceset_text} = Wx::StaticText->new($self, -1, "Deviceset:",wxDefaultPosition,[105,-1]);
    my $deviceset_field = $self->{deviceset_field} = Wx::TextCtrl->new($self, -1, "",wxDefaultPosition,  [230,-1], wxTE_READONLY);
    
    my $device_text = $self->{device_text} = Wx::StaticText->new($self, -1, "Device:",wxDefaultPosition,[105,-1]);
    my $device_field = $self->{device_field} = Wx::TextCtrl->new($self, -1, "",wxDefaultPosition,  [230,-1], wxTE_READONLY);
    
    my $package_text = $self->{package_text} = Wx::StaticText->new($self, -1, "Package:",wxDefaultPosition,[105,-1]);
    my $package_field = $self->{package_field} = Wx::TextCtrl->new($self, -1, "",wxDefaultPosition,  [230,-1], wxTE_READONLY);
    
    my $height_text = $self->{height_text} = Wx::StaticText->new($self, -1, "Layer height:",wxDefaultPosition,[100,-1]);
    my $height_field = $self->{height_field} = Wx::TextCtrl->new($self, -1, "",wxDefaultPosition,  [230,-1]);
    
    my $partheight_text = $self->{partheight_text} = Wx::StaticText->new($self, -1, "Part height:",wxDefaultPosition,[105,-1]);
    my $partheight_field = $self->{partheight_field} = Wx::TextCtrl->new($self, -1, "",wxDefaultPosition,  [230,-1]);
    
    
    my $position_text = $self->{position_text} = Wx::StaticText->new($self, -1, "Position:",wxDefaultPosition,[85,-1]);
    my $x_text = $self->{x_text} = Wx::StaticText->new($self, -1, "X:",wxDefaultPosition,[15,-1]);
    my $x_field = $self->{x_field} = Wx::TextCtrl->new($self, -1, "",wxDefaultPosition,  [60,-1]);
    
    my $y_text = $self->{y_text} = Wx::StaticText->new($self, -1, "Y:",wxDefaultPosition,[15,-1]);
    my $y_field = $self->{y_field} = Wx::TextCtrl->new($self, -1, "",wxDefaultPosition,  [60,-1]);
    
    my $z_text = $self->{z_text} = Wx::StaticText->new($self, -1, "Z:",wxDefaultPosition,[15,-1]);
    my $z_field = $self->{z_field} = Wx::TextCtrl->new($self, -1, "",wxDefaultPosition,  [60,-1]);
    
    my $rotation_text = $self->{rotation_text} = Wx::StaticText->new($self, -1, "Rotation:",wxDefaultPosition,[85,-1]);
    my $xr_text = $self->{xr_text} = Wx::StaticText->new($self, -1, "X:",wxDefaultPosition,[15,-1]);
    my $xr_field = $self->{xr_field} = Wx::TextCtrl->new($self, -1, "",wxDefaultPosition,  [60,-1]);
    
    my $yr_text = $self->{yr_text} = Wx::StaticText->new($self, -1, "Y:",wxDefaultPosition,[15,-1]);
    my $yr_field = $self->{yr_field} = Wx::TextCtrl->new($self, -1, "",wxDefaultPosition,  [60,-1]);
    
    my $zr_text = $self->{zr_text} = Wx::StaticText->new($self, -1, "Z:",wxDefaultPosition,[15,-1]);
    my $zr_field = $self->{zr_field} = Wx::TextCtrl->new($self, -1, "",wxDefaultPosition,  [60,-1]);
    
    my $empty_text = $self->{empty_text} = Wx::StaticText->new($self, -1, "",wxDefaultPosition,[15,-1]);
    
    my $btn_xp = $self->{btn_xp} = Wx::Button->new($self, -1, "+", wxDefaultPosition, [20,20], wxBU_LEFT);
    my $btn_xm = $self->{btn_xm} = Wx::Button->new($self, -1, "-", wxDefaultPosition, [20,20], wxBU_LEFT);
    my $btn_yp = $self->{btn_yp} = Wx::Button->new($self, -1, "+", wxDefaultPosition, [20,20], wxBU_LEFT);
    my $btn_ym = $self->{btn_ym} = Wx::Button->new($self, -1, "-", wxDefaultPosition, [20,20], wxBU_LEFT);
    my $btn_zp = $self->{btn_zp} = Wx::Button->new($self, -1, "+", wxDefaultPosition, [20,20], wxBU_LEFT);
    my $btn_zm = $self->{btn_zm} = Wx::Button->new($self, -1, "-", wxDefaultPosition, [20,20], wxBU_LEFT);
    
    my $sizer_x = Wx::FlexGridSizer->new( 1, 2, 5, 5);
    my $sizer_y = Wx::FlexGridSizer->new( 1, 2, 5, 5);
    my $sizer_z = Wx::FlexGridSizer->new( 1, 2, 5, 5);
    
    
    $sizer_x->Add($self->{btn_xm}, 1,wxTOP, 0);
    $sizer_x->Add($self->{btn_xp}, 1,wxTOP, 0);

    $sizer_y->Add($self->{btn_ym}, 1,wxTOP, 0);
    $sizer_y->Add($self->{btn_yp}, 1,wxTOP, 0);

    $sizer_z->Add($self->{btn_zm}, 1,wxTOP, 0);
    $sizer_z->Add($self->{btn_zp}, 1,wxTOP, 0);
    
    # settings sizer
    my $settings_sizer_main = Wx::StaticBoxSizer->new($self->{staticbox} = Wx::StaticBox->new($self, -1, "Part Settings"),wxVERTICAL);
    my $settings_sizer_main_grid = Wx::FlexGridSizer->new( 3, 1, 5, 5);
    my $settings_sizer_sttings = Wx::FlexGridSizer->new( 7, 2, 5, 5);
    my $settings_sizer_positions = Wx::FlexGridSizer->new( 3, 7, 5, 5);
    my $settings_sizer_buttons = Wx::FlexGridSizer->new( 1, 1, 5, 5);
    
    $settings_sizer_main->Add($settings_sizer_main_grid, 0,wxTOP, 0);
    
    $settings_sizer_main_grid->Add($settings_sizer_sttings, 0,wxTOP, 0);
    $settings_sizer_main_grid->Add($settings_sizer_positions, 0,wxTOP, 0);
    $settings_sizer_main_grid->Add($settings_sizer_buttons, 0,wxTOP, 0);
    
    $settings_sizer_sttings->Add($self->{name_text}, 1,wxTOP, 0);
    $settings_sizer_sttings->Add($self->{name_field}, 1,wxTOP, 0);
    $settings_sizer_sttings->Add($self->{library_text}, 1,wxTOP, 0);
    $settings_sizer_sttings->Add($self->{library_field}, 1,wxTOP, 0);
    $settings_sizer_sttings->Add($self->{deviceset_text}, 1,wxTOP, 0);
    $settings_sizer_sttings->Add($self->{deviceset_field}, 1,wxTOP, 0);
    $settings_sizer_sttings->Add($self->{device_text}, 1,wxTOP, 0);
    $settings_sizer_sttings->Add($self->{device_field}, 1,wxTOP, 0);
    $settings_sizer_sttings->Add($self->{package_text}, 1,wxTOP, 0);
    $settings_sizer_sttings->Add($self->{package_field}, 1,wxTOP, 0);
    $settings_sizer_sttings->Add($self->{height_text}, 1,wxTOP, 0);
    $settings_sizer_sttings->Add($self->{height_field}, 1,wxTOP, 0);
    $settings_sizer_sttings->Add($self->{partheight_text}, 1,wxTOP, 0);
    $settings_sizer_sttings->Add($self->{partheight_field}, 1,wxTOP, 0);
    
    $settings_sizer_positions->Add($self->{position_text}, 1,wxTOP, 0);
    $settings_sizer_positions->Add($self->{x_text}, 1,wxTOP, 0);
    $settings_sizer_positions->Add($self->{x_field}, 1,wxTOP, 0);
    $settings_sizer_positions->Add($self->{y_text}, 1,wxTOP, 0);
    $settings_sizer_positions->Add($self->{y_field}, 1,wxTOP, 0);
    $settings_sizer_positions->Add($self->{z_text}, 1,wxTOP, 0);
    $settings_sizer_positions->Add($self->{z_field}, 1,wxTOP, 0);
    
    $settings_sizer_positions->Add($self->{empty_text}, 1,wxTOP, 0);
    $settings_sizer_positions->Add($self->{empty_text}, 1,wxTOP, 0);
    $settings_sizer_positions->Add($sizer_x, 1,wxTOP, 0);
    $settings_sizer_positions->Add($self->{empty_text}, 1,wxTOP, 0);
    $settings_sizer_positions->Add($sizer_y, 1,wxTOP, 0);
    $settings_sizer_positions->Add($self->{empty_text}, 1,wxTOP, 0);
    $settings_sizer_positions->Add($sizer_z, 1,wxTOP, 0);
    
    $settings_sizer_positions->Add($self->{rotation_text}, 1,wxTOP, 0);
    $settings_sizer_positions->Add($self->{xr_text}, 1,wxTOP, 0);
    $settings_sizer_positions->Add($self->{xr_field}, 1,wxTOP, 0);
    $settings_sizer_positions->Add($self->{yr_text}, 1,wxTOP, 0);
    $settings_sizer_positions->Add($self->{yr_field}, 1,wxTOP, 0);
    $settings_sizer_positions->Add($self->{zr_text}, 1,wxTOP, 0);
    $settings_sizer_positions->Add($self->{zr_field}, 1,wxTOP, 0);
    
    
    my $btn_save_part = $self->{btn_save_part} = Wx::Button->new($self, -1, "Save Part", wxDefaultPosition, wxDefaultSize, wxBU_LEFT);
    $settings_sizer_buttons->Add($btn_save_part, 0);
    $btn_save_part->SetFont($Slic3r::GUI::small_font);
    
    # lower buttons 
    my $btn_save_netlist = $self->{btn_save_netlist} = Wx::Button->new($self, -1, "Save netlist", wxDefaultPosition, wxDefaultSize, wxBU_LEFT);
    
    # lower buttons sizer
    my $buttons_sizer_bottom = Wx::FlexGridSizer->new( 1, 3, 5, 5);
    $buttons_sizer_bottom->Add($btn_save_netlist, 0);
    $btn_save_netlist->SetFont($Slic3r::GUI::small_font);
    
    # left pane with tree
    my $left_sizer = Wx::BoxSizer->new(wxVERTICAL);
    $left_sizer->Add($buttons_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 5);
    $left_sizer->Add($tree, 1, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 5);
    $left_sizer->Add($buttons_sizer_mid, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 5);
    $left_sizer->Add($settings_sizer_main, 0, wxEXPAND | wxALL| wxRIGHT | wxTOP, 5);
    $left_sizer->Add($buttons_sizer_bottom, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);
    
    # slider for choosing layer
    my $slider = $self->{slider} = Wx::Slider->new(
        $self, -1,
        0,                              # default
        0,                              # min
        # we set max to a bogus non-zero value because the MSW implementation of wxSlider
        # will skip drawing the slider if max <= min:
        1,                              # max
        wxDefaultPosition,
        wxDefaultSize,
        wxVERTICAL | &Wx::wxSL_INVERSE,
    );
    
    my $sliderconf = $self->{sliderconf} = 0;
    
    # label for slider
    my $z_label = $self->{z_label} = Wx::StaticText->new($self, -1, "", wxDefaultPosition, [40,-1], wxALIGN_CENTRE_HORIZONTAL);
    $z_label->SetFont($Slic3r::GUI::small_font);
    
    # slider sizer
    my $vsizer = Wx::BoxSizer->new(wxVERTICAL);
    $vsizer->Add($slider, 1, wxALL | wxEXPAND | wxALIGN_CENTER, 3);
    $vsizer->Add($z_label, 0, wxALL | wxEXPAND | wxALIGN_CENTER, 3);
    
    # right pane with preview canvas
    my $canvas;
    if ($Slic3r::GUI::have_OpenGL) {
        $canvas = $self->{canvas} = Slic3r::GUI::Electronics3DScene->new($self);
        $canvas->enable_picking(1);
        $canvas->select_by('volume');
        
        $canvas->on_select(sub {
            my ($volume_idx) = @_;
            if(defined $self->{object_list}) {
	            my $partID = $self->{object_list}[$volume_idx];
	            if(defined $partID) {
	            	$self->reload_tree($partID);	
	            }
            }
        });
                
        $canvas->load_object($self->{model_object}, undef, [0]);
        $canvas->set_auto_bed_shape;
        $canvas->SetSize([500,500]);
        $canvas->zoom_to_volumes;
        # init canvas
        $self->print($print);
        $self->reload_print;
    }
    
    #set box sizer
    $self->{sizer} = Wx::BoxSizer->new(wxHORIZONTAL);
    $self->{sizer}->Add($left_sizer, 0, wxEXPAND | wxALL , 0);
    $self->{sizer}->Add($canvas, 1, wxEXPAND | wxALL, 1) if $canvas;
    $self->{sizer}->Add($vsizer, 0, wxTOP | wxBOTTOM | wxEXPAND, 5);
    
    $self->SetSizer($self->{sizer});
    $self->{sizer}->SetSizeHints($self);
    
    # attach events
    EVT_SLIDER($self, $slider, sub {
        $self->sliderMoved if $self->enabled;
    });

    # Item tree cant collapse
    EVT_TREE_ITEM_COLLAPSING($self, $tree, sub {
        my ($self, $event) = @_;
        $event->Veto;
    });
    
    EVT_TREE_SEL_CHANGED($self, $tree, sub {
        my ($self, $event) = @_;
        return if $self->{disable_tree_sel_changed_event};
        $self->selection_changed;
    });
    
    EVT_BUTTON($self, $self->{btn_load_netlist}, sub { 
        $self->loadButtonPressed;
    });
    
    EVT_BUTTON($self, $self->{btn_place_part}, sub { 
        $self->placeButtonPressed; 
    });
    
    EVT_BUTTON($self, $self->{btn_remove_part}, sub { 
        $self->removeButtonPressed; 
    });
    
    EVT_BUTTON($self, $self->{btn_save_part}, sub { 
        $self->savePartButtonPressed; 
    });
    
    EVT_BUTTON($self, $self->{btn_save_netlist}, sub { 
        $self->saveButtonPressed;
    });
    
    EVT_BUTTON($self, $self->{btn_xp}, sub { 
        $self->movePart($self->{config}->{_}{move_step},0,0);
    });
    
    EVT_BUTTON($self, $self->{btn_xm}, sub { 
        $self->movePart($self->{config}->{_}{move_step}*-1,0,0);
    });
    
    EVT_BUTTON($self, $self->{btn_yp}, sub { 
        $self->movePart(0,$self->{config}->{_}{move_step},0);
    });
    
    EVT_BUTTON($self, $self->{btn_ym}, sub { 
        $self->movePart(0,$self->{config}->{_}{move_step}*-1,0);
    });
    
    EVT_BUTTON($self, $self->{btn_zp}, sub { 
        $self->movePart(0,0,1);
    });
    
    EVT_BUTTON($self, $self->{btn_zm}, sub { 
        $self->movePart(0,0,-1);
    });
    
    $self->reload_tree;

    return $self;
}

#######################################################################
# Purpose    : Writes the default configutation
# Parameters : $configfile to write
# Returns    : none
# Comment     : 
#######################################################################
sub createDefaultConfig {
    my $self = shift;
    my ($configfile) = @_;
    $self->{config}->{_}{show_parts_on_higher_layers} = 0;
    $self->{config}->{_}{move_step} = 0.1;
    $self->{config}->{_}{footprint_extruder} = 0;
    $self->{config}->{_}{part_extruder} = 0;
    
    $self->{config}->{offset}{margin} = 0.5;
    
    $self->{config}->{chip_height}{default} = 1;
    Slic3r::Config->write_ini($configfile, $self->{config});
}

#######################################################################
# Purpose    : Reloads canvas when necesary
# Parameters : none
# Returns    : none
# Comment     : 
#######################################################################
sub sliderMoved {
    my $self = shift;
    my $height =  $self->{layers_z}[$self->{slider}->GetValue];
    my $changed = 0;
    
    my $partlist = $self->{schematic}->getPartlist();
    for my $part (@{$partlist}) {
    	if (($part->isPlaced && 
            (!$part->isVisible && $part->getPosition()->z-$part->getFootprintHeight <= $height) ||
            ($part->isVisible && $part->getPosition()->z-$part->getFootprintHeight > $height))) {
            $changed = 1;
        }
    }
    
    $self->set_z($height);
    if ($changed == 1) {
        $self->reload_print;
    }
}
            
#######################################################################
# Purpose    : Reloads the print on the canvas
# Parameters : none
# Returns    : none
# Comment     :
#######################################################################
sub reload_print {
    my ($self) = @_;
    $self->canvas->reset_objects;
    $self->_loaded(0);
    
    my $partlist = $self->{schematic}->getPartlist();
    for my $part (@{$partlist}) {
    	$part->setVisibility(0);
    }
              
    $self->render_print;
}

#######################################################################
# Purpose    : renders the print preview and the objects on the canvas
# Parameters : none
# Returns    : undef if not loaded
# Comment     : First loads Print, second footprints and third parts
#######################################################################
sub render_print {
    my ($self) = @_;
    return if $self->_loaded;
    
    # we require that there's at least one object and the posSlice step
    # is performed on all of them (this ensures that _shifted_copies was
    # populated and we know the number of layers)
    if (!$self->print->object_step_done(STEP_SLICE)) {
        $self->enabled(0);
        $self->{slider}->Hide;
        $self->canvas->Refresh;  # clears canvas
        return;
    }
    
    # configure slider
    if (!$self->{sliderconf}) {
        my %z = ();  # z => 1
        foreach my $object (@{$self->{print}->objects}) {
            foreach my $layer (@{$object->layers}, @{$object->support_layers}) {
                $z{$layer->print_z} = 1;
            }
        }
        $self->enabled(1);
        $self->{layers_z} = [ sort { $a <=> $b } keys %z ];
        $self->{slider}->SetRange(0, scalar(@{$self->{layers_z}})-1);
        if ((my $z_idx = $self->{slider}->GetValue) <= $#{$self->{layers_z}} && $self->{slider}->GetValue != 0) {
            $self->set_z($self->{layers_z}[$z_idx]);
        } else {
            $self->{slider}->SetValue(scalar(@{$self->{layers_z}})-1);
            $self->set_z($self->{layers_z}[-1]) if @{$self->{layers_z}};
        }
        $self->{slider}->Show;
        $self->Layout;
        $self->{sliderconf} = 1;
    }
    
    # reset lookup array
    my @lookup_table;
    
    # load objects
    if ($self->IsShown) {
        # load skirt and brim
        $self->canvas->load_print_toolpaths($self->print);
        
        foreach my $object (@{$self->print->objects}) {
            $self->canvas->load_print_object_toolpaths($object);
            
        }
        
        my $height =  $self->{layers_z}[$self->{slider}->GetValue];
        
        # Display SMD models
        foreach my $part (@{$self->{schematic}->getPartlist()}) {
        	if($part->isPlaced()) {
        		# part visible?
        		if ($part->getPosition()->z-$part->getFootprintHeight() <= $height || $self->{config}->{_}{show_parts_on_higher_layers}) {
        			$part->setVisibility(1);
        			my $mesh = $part->getMesh();
        			$mesh->translate($self->{rootOffset}->x, $self->{rootOffset}->y, 0);
        			my $object_id = $self->canvas->load_electronic_part($mesh);
        			$lookup_table[$object_id] = $part->getPartID;
        		}else{
        			$part->setVisibility(0);
        		}
        	}
        }
        
        $self->{object_list} = \@lookup_table;
        
        # Display rubber-banding
#        for my $part (@{$self->{schematic}->{partlist}}) {
#	        if ($part->{shown}) {
#	        	
#	        }
#	    }
        
        $self->set_z($height) if $self->enabled;
        
        if (!$self->{sliderconf}) {
            $self->canvas->zoom_to_volumes;
        }
        $self->_loaded(1);
    }
}

#######################################################################
# Purpose    : Places a part to a position on canvas
# Parameters : part and x,y,z positions
# Returns    : none
# Comment     : For mouse placement, also calculates the offset
#######################################################################
sub placePart {
    my $self = shift;
    my ($part, $x, $y, $z) = @_;
    $part->setPosition($x-$self->{rootOffset}->x, $y-$self->{rootOffset}->y, $z);
    $part->setPlaced(1);
    $self->displayPart($part);
    $self->reload_tree($part->getPartID());
    
    # trigger slicing steps to update modifications;
    $self->{print}->objects->[$self->{obj_idx}]->invalidate_step(STEP_PERIMETERS);
    $self->{plater}->stop_background_process;
    $self->{plater}->start_background_process(1);
    
}

#######################################################################
# Purpose    : Displays a Part and its footprint on the canvas
# Parameters : part to display
# Returns    : none
# Comment     : When the part already exists on canvas it will be deleted
#######################################################################
sub displayPart {
    my $self = shift;
    my ($part) = @_;
    if ($part->isPlaced()) {
    	my $position = $part->getPosition();
        $part->setFootprintHeight($self->get_layer_thickness($position->z));
    }
}

#######################################################################
# Purpose    : Removes a part and its footprint from canvas
# Parameters : part or volume_id
# Returns    : none
# Comment     : 
#######################################################################
sub removePart {
    my ($self, $part) = @_;
    
    $part->setPlaced(0);
    $part->resetPosition;
    $self->reload_tree($part->getPartID);
}

#######################################################################
# Purpose    : Reloads the model tree
# Parameters : currently selected volume
# Returns    : none
# Comment     : 
#######################################################################
sub reload_tree {
    my ($self, $selected_volume_idx) = @_;
    
    $selected_volume_idx //= -1;
    
    my $object  = $self->{model_object};
    my $tree    = $self->{tree};
    my $rootId  = $tree->GetRootItem;
    
    # despite wxWidgets states that DeleteChildren "will not generate any events unlike Delete() method",
    # the MSW implementation of DeleteChildren actually calls Delete() for each item, so
    # EVT_TREE_SEL_CHANGED is being called, with bad effects (the event handler is called; this 
    # subroutine is never continued; an invisible EndModal is called on the dialog causing Plater
    # to continue its logic and rescheduling the background process etc. GH #2774)
    $self->{disable_tree_sel_changed_event} = 1;
    $tree->DeleteChildren($rootId);
    $self->{disable_tree_sel_changed_event} = 0;
    $self->reload_print;
    
    my $selectedId = $rootId;
    my $itemId;
    foreach my $volume_id (0..$#{$object->volumes}) {
        my $volume = $object->volumes->[$volume_id];
        
        my $icon = $volume->modifier ? ICON_MODIFIERMESH : ICON_SOLIDMESH;
        $itemId = $tree->AppendItem($rootId, $volume->name || $volume_id, $icon);
        $tree->SetPlData($itemId, {
            type        => 'volume',
            volume_id   => $volume_id,
            volume      => $volume,
        });
    }
    
    if (defined $self->{schematic}) {
    	my $partlist = $self->{schematic}->getPartlist();
        if ($#{$partlist} > 0) {
        	
        	# placed SMDs
            my $eIcon = ICON_PCB;
            my $eItemId = $tree->AppendItem($rootId, "placed");
            $tree->SetPlData($eItemId, {
                type        => 'placed',
                volume_id   => 0,
            });
            foreach my $part (@{$partlist}) {
            	if($part->isPlaced()) {
                    $itemId = $tree->AppendItem($eItemId, $part->getName(), $eIcon);
                    $tree->SetPlData($itemId, {
                        type        => 'part',
                        partID		=> $part->getPartID(),
                        part        => $part,
                    });
                    if($part->getPartID() == $selected_volume_idx) {
			            $selectedId = $itemId;
			        }
                }
            }
        
	    	#unplaced SMDs
            $eItemId = $tree->AppendItem($rootId, "unplaced");
            $tree->SetPlData($eItemId, {
                type        => 'unplaced',
                volume_id   => 0,
            });
            foreach my $part (@{$partlist}) {
            	if(!$part->isPlaced()) {
                    $itemId = $tree->AppendItem($eItemId, $part->getName(), $eIcon);
                    $tree->SetPlData($itemId, {
                        type        => 'part',
                        part        => $part,
                    });
                    if($part->getPartID() == $selected_volume_idx) {
			            $selectedId = $itemId;
			        }
                }
            }
        }
    }
    $tree->ExpandAll;
    
    $self->{tree}->SelectItem($selectedId);
    
    my $a = Slic3r::Pointf3->new(0, 0, 0);
    my $b = Slic3r::Pointf3->new(50, 40, 10);
    
    $self->canvas->draw_line($b, $a, 0.5, [0, 1, 0, 0.9]);
    $self->canvas->draw_line(Slic3r::Pointf3->new(0, 0, 20), Slic3r::Pointf3->new(50, 20, 20), 0.5);
}

#######################################################################
# Purpose    : Gets the selected volume form the model tree
# Parameters : none
# Returns    : volumeid or undef
# Comment     :
#######################################################################
sub get_selection {
    my ($self) = @_;
    
    my $nodeId = $self->{tree}->GetSelection;
    if ($nodeId->IsOk) {
        return $self->{tree}->GetPlData($nodeId);
    }
    return undef;
}

#######################################################################
# Purpose    : Changes the GUI when the seletion in the model tree changes
# Parameters : none
# Returns    : none
# Comment     :
#######################################################################
sub selection_changed {
    my ($self) = @_;
    my $selection = $self->get_selection;
    if ($selection->{type} eq 'part') {
    	my $part = $selection->{part};
    	# jump to corresponding layer
    	if($part->isPlaced) {
    		if($self->get_z < $part->getPosition->z || $self->get_z > $part->getPosition->z + $part->getPartHeight) {
    			
    			for my $i (0 .. $#{$self->{layers_z}}) {
    				print "part position: " . $part->getPosition->z . " i: " . $i . "\n"; 
    				if($self->{layers_z}[$i] >= $part->getPosition->z) {
    					$self->{slider}->SetValue($i);
    					print "after setting, getValue: " . $self->{slider}->GetValue . "\n";
    					$self->sliderMoved;
    					last;
    				}
    			}
    		}
    	}
    	$self->showPartInfo($part);
    }else {
        $self->clearPartInfo;
    }
}

#######################################################################
# Purpose    : Gets the current z position of the canvas
# Parameters : none
# Returns    : z position
# Comment     :
#######################################################################
sub get_z {
    my ($self) = @_;
    return $self->{layers_z}[$self->{slider}->GetValue];
}

#######################################################################
# Purpose    : Sets the z position on the canvas
# Parameters : z position
# Returns    : undef if canvas is not active
# Comment     :
#######################################################################
sub set_z {
    my ($self, $z) = @_;
    
    return if !$self->enabled;
    $self->{z_label}->SetLabel(sprintf '%.2f', $z);
    $self->canvas->set_toolpaths_range(0, $z);
    $self->canvas->Refresh if $self->IsShown;
}

#######################################################################
# Purpose    : Gets the thickness of the current layer
# Parameters : z position
# Returns    : layer thickness
# Comment     :
#######################################################################
sub get_layer_thickness {
    my $self = shift;
    my ($z) = @_;
    my $i = 0;
    for my $layer (@{$self->{layers_z}}) {
        if ($z <= $layer) {
            if ($i == 0) {
                return $layer;
            } else {
                return sprintf "%.2f", $layer - $self->{layers_z}[$i-1];
            }
        }
        $i += 1;
    }
}

#######################################################################
# Purpose    : Shows the part info in the GUI
# Parameters : part to display
# Returns    : none
# Comment     :
#######################################################################
sub showPartInfo {
    my $self = shift;
    my ($part) = @_;
    $self->clearPartInfo;
    $self->{name_field}->SetValue($part->getName());
    $self->{library_field}->SetValue($part->getLibrary());
    $self->{deviceset_field}->SetValue($part->getDeviceset());
    $self->{device_field}->SetValue($part->getDevice());
    $self->{package_field}->SetValue($part->getPackage());
    $self->{height_field}->SetValue($part->getFootprintHeight());
    $self->{partheight_field}->SetValue($part->getPartHeight());
    my $position = $part->getPosition();
    $self->{x_field}->SetValue($position->x);
    $self->{y_field}->SetValue($position->y);
    $self->{z_field}->SetValue($position->z);
    my $rotation = $part->getRotation();
    $self->{xr_field}->SetValue($rotation->x);
    $self->{yr_field}->SetValue($rotation->y);
    $self->{zr_field}->SetValue($rotation->z);
}

#######################################################################
# Purpose    : Clears the part info
# Parameters : none
# Returns    : none
# Comment     :
#######################################################################
sub clearPartInfo {
    my $self = shift;
    $self->{name_field}->SetValue("");
    $self->{library_field}->SetValue("");
    $self->{deviceset_field}->SetValue("");
    $self->{device_field}->SetValue("");
    $self->{package_field}->SetValue("");
    $self->{height_field}->SetValue("");
    $self->{partheight_field}->SetValue("");
    $self->{x_field}->SetValue("");
    $self->{y_field}->SetValue("");
    $self->{z_field}->SetValue("");
    $self->{xr_field}->SetValue("");
    $self->{yr_field}->SetValue("");
    $self->{zr_field}->SetValue("");
}

#######################################################################
# Purpose    : Saves the partinfo of the displayed part
# Parameters : part to save
# Returns    : none
# Comment     :
#######################################################################
sub savePartInfo {
    my $self = shift;
    my ($part) = @_;
    
	$part->setPartHeight($self->{partheight_field}->GetValue);

    if (!($self->{x_field}->GetValue eq "") && !($self->{y_field}->GetValue eq "") && !($self->{z_field}->GetValue eq "")) {
    	$part->setPosition($self->{x_field}->GetValue, $self->{y_field}->GetValue, $self->{z_field}->GetValue);
    }
    if (!($self->{xr_field}->GetValue eq "") && !($self->{yr_field}->GetValue eq "") && !($self->{zr_field}->GetValue eq "")) {
    	$part->setRotation($self->{xr_field}->GetValue, $self->{yr_field}->GetValue, $self->{zr_field}->GetValue);
    }
}

#######################################################################
# Purpose    : Event for load button
# Parameters : $file to load
# Returns    : none
# Comment     : calls the method to read the file
#######################################################################
sub loadButtonPressed {
    my $self = shift;
    my ($file) = @_;
    
    if (!$file) {
        my $dlg = Wx::FileDialog->new(
            $self, 
            'Select schematic to load:',
            '',
            '',
            &Slic3r::GUI::FILE_WILDCARDS->{sch}, 
            wxFD_OPEN | wxFD_FILE_MUST_EXIST);
        return unless $dlg->ShowModal == wxID_OK;
        $file = Slic3r::decode_path($dlg->GetPaths);
        $dlg->Destroy;
    }

    Slic3r::Electronics::Electronics->readFile($file,$self->{schematic}, $self->{config});
    
    # register partlist for slicing modifications in Print->Object
    #$self->{print}->objects->[$self->{obj_idx}]->registerElectronicPartList($self->{schematic}->{partlist});
    $self->{print}->objects->[$self->{obj_idx}]->registerSchematic($self->{schematic});

    $self->reload_tree;
}

#######################################################################
# Purpose    : Event for place button
# Parameters : none
# Returns    : none
# Comment     : sets the place variable to the current selection
#######################################################################
sub placeButtonPressed {
    my $self = shift;
    $self->{place} = $self->get_selection;
}

#######################################################################
# Purpose    : Returns the current item to place
# Parameters : none
# Returns    : item to place
# Comment     :
#######################################################################
sub get_place {
    my $self = shift;
    return $self->{place};
}

#######################################################################
# Purpose    : Sets the item to place
# Parameters : item to place
# Returns    : none
# Comment     :
#######################################################################
sub set_place {
    my $self = shift;
    my ($value) = @_;
    $self->{place} = $value;
}

#######################################################################
# Purpose    : Event for remove button
# Parameters : none
# Returns    : none
# Comment     : calls the remove function
#######################################################################
sub removeButtonPressed {
    my $self = shift;
    my $selection = $self->get_selection;
    if ($selection->{type} eq 'part') {
    	$self->removePart($selection->{part});
    }
}

#######################################################################
# Purpose    : Event for save part button
# Parameters : none
# Returns    : none
# Comment     : saves the partinfo
#######################################################################
sub savePartButtonPressed {
    my $self = shift;
    my $selection = $self->get_selection;
    if ($selection->{type} eq 'part') {
        my $part = $selection->{part};
        $self->savePartInfo($part);
        $self->displayPart($part);
        $self->reload_tree($part->getPartID());
    }
    
    # trigger slicing steps to update modifications;
    $self->{print}->objects->[$self->{obj_idx}]->invalidate_step(STEP_SLICE);
    # calls schedule to update the toolpath to give the user an opportunity for multiple position updates
    $self->{plater}->schedule_background_process;
}

#######################################################################
# Purpose    : Event for save button
# Parameters : none
# Returns    : none
# Comment     : Calls Slic3r::Electronics::Electronics->writeFile
#######################################################################
sub saveButtonPressed {
    my $self = shift;
    #my ($base,$path,$type) = fileparse($self->{schematic}->{filename},('.sch','.SCH','3de','.3DE'));
    #if (Slic3r::Electronics::Electronics->writeFile($self->{schematic},$self->{config})) {
    #    Wx::MessageBox('File saved as '.$base.'.3de','Saved', Wx::wxICON_INFORMATION | Wx::wxOK,undef)
    #} else {
    #    Wx::MessageBox('Saving failed','Failed',Wx::wxICON_ERROR | Wx::wxOK,undef)
    #}
}

#######################################################################
# Purpose    : MovesPart with Buttons
# Parameters : x, y and z coordinates
# Returns    : none
# Comment     : moves Z by layer thickness
#######################################################################
sub movePart {
    my $self = shift;
    my ($x,$y,$z) = @_;
    
    my $selection = $self->get_selection;
    if ($selection->{type} eq 'part') {
        my $part = $selection->{part};
        
        $self->{plater}->stop_background_process;
        my $oldPos = $part->getPosition;
        $part->setPosition($oldPos->x + $x, $oldPos->y + $y, $oldPos->z + $self->get_layer_thickness($oldPos->z)*$z);
        
        $self->showPartInfo($part);
        $self->displayPart($part);
        $self->reload_tree($part->getPartID);
                
        # trigger slicing steps to update modifications;
		$self->{print}->objects->[$self->{obj_idx}]->invalidate_step(STEP_SLICE);
		$self->{plater}->start_background_process(1);

    }
}

1;
