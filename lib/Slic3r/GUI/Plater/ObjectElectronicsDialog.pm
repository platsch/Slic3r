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
# Commet     :
#######################################################################
sub new {
    my $class = shift;
    my ($parent, $print, %params) = @_;
    my $self = $class->SUPER::new($parent, -1, "3D Electronics for " . $params{object}->name, wxDefaultPosition, [800,600], &Wx::wxMAXIMIZE | &Wx::wxDEFAULT_FRAME_STYLE);
    $self->{$_} = $params{$_} for keys %params;
    
    $self->{tabpanel} = Wx::Notebook->new($self, -1, wxDefaultPosition, wxDefaultSize, wxNB_TOP | wxTAB_TRAVERSAL);
    $self->{tabpanel}->AddPage($self->{parts} = Slic3r::GUI::Plater::ElectronicsPanel->new($self->{tabpanel},$print, model_object => $params{model_object}, schematic => $params{schematic}), "Electronics");
        
    my $sizer = Wx::BoxSizer->new(wxVERTICAL);
    $sizer->Add($self->{tabpanel}, 1, wxEXPAND | wxTOP | wxLEFT | wxRIGHT, 10);
    
    $self->SetSizer($sizer);
    $self->SetMinSize($self->GetSize);
    
    return $self;
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
    wxTheApp);
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
# Commet     : Main Panel for 3D Electronics
#######################################################################
sub new {
    my $class = shift;
    my ($parent, $print, %params) = @_;
    my $self = $class->SUPER::new($parent, -1, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    
    my $object = $self->{model_object} = $params{model_object};
    my $schematic = $self->{schematic} = $params{schematic};
    my $place = $self->{place} = 0;
    $self->{model_object}->update_bounding_box;
    my $root_offset = $self->{schematic}->{root_offset} = $self->{model_object}->_bounding_box->center;
    
    my $configfile ||= Slic3r::decode_path(Wx::StandardPaths::Get->GetUserDataDir . "/electronics/electronics.ini");
    my $config = $self->{config};
    if (-f $configfile) {
        $self->{config} = eval { Slic3r::Config->read_ini($configfile) };
    } else {
        $self->createDefaultConfig($configfile);
    }
    
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
    my $name_field = $self->{name_field} = Wx::TextCtrl->new($self, -1, "",wxDefaultPosition, [230,-1]);
    
    my $library_text = $self->{library_text} = Wx::StaticText->new($self, -1, "Library:",wxDefaultPosition,[105,-1]);
    my $library_field = $self->{library_field} = Wx::TextCtrl->new($self, -1, "",wxDefaultPosition,  [230,-1]);
    
    my $deviceset_text = $self->{deviceset_text} = Wx::StaticText->new($self, -1, "Deviceset:",wxDefaultPosition,[105,-1]);
    my $deviceset_field = $self->{deviceset_field} = Wx::TextCtrl->new($self, -1, "",wxDefaultPosition,  [230,-1]);
    
    my $device_text = $self->{device_text} = Wx::StaticText->new($self, -1, "Device:",wxDefaultPosition,[105,-1]);
    my $device_field = $self->{device_field} = Wx::TextCtrl->new($self, -1, "",wxDefaultPosition,  [230,-1]);
    
    my $package_text = $self->{package_text} = Wx::StaticText->new($self, -1, "Package:",wxDefaultPosition,[105,-1]);
    my $package_field = $self->{package_field} = Wx::TextCtrl->new($self, -1, "",wxDefaultPosition,  [230,-1]);
    
    my $height_text = $self->{height_text} = Wx::StaticText->new($self, -1, "Layer height:",wxDefaultPosition,[100,-1]);
    my $height_field = $self->{height_field} = Wx::TextCtrl->new($self, -1, "",wxDefaultPosition,  [230,-1]);
    
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
    
    my $componentsize_text = $self->{componentsize_text} = Wx::StaticText->new($self, -1, "Component size:",wxDefaultPosition,[85,-1]);
    my $xs_text = $self->{xs_text} = Wx::StaticText->new($self, -1, "X:",wxDefaultPosition,[15,-1]);
    my $xs_field = $self->{xs_field} = Wx::TextCtrl->new($self, -1, "",wxDefaultPosition,  [60,-1]);
    
    my $ys_text = $self->{ys_text} = Wx::StaticText->new($self, -1, "Y:",wxDefaultPosition,[15,-1]);
    my $ys_field = $self->{ys_field} = Wx::TextCtrl->new($self, -1, "",wxDefaultPosition,  [60,-1]);
    
    my $zs_text = $self->{zs_text} = Wx::StaticText->new($self, -1, "Z:",wxDefaultPosition,[15,-1]);
    my $zs_field = $self->{zs_field} = Wx::TextCtrl->new($self, -1, "",wxDefaultPosition,  [60,-1]);
    
    my $componentpos_text = $self->{componentpos_text} = Wx::StaticText->new($self, -1, "Component position:",wxDefaultPosition,[85,-1]);
    my $xp_text = $self->{xp_text} = Wx::StaticText->new($self, -1, "X:",wxDefaultPosition,[15,-1]);
    my $xp_field = $self->{xp_field} = Wx::TextCtrl->new($self, -1, "",wxDefaultPosition,  [60,-1]);
    
    my $yp_text = $self->{yp_text} = Wx::StaticText->new($self, -1, "Y:",wxDefaultPosition,[15,-1]);
    my $yp_field = $self->{yp_field} = Wx::TextCtrl->new($self, -1, "",wxDefaultPosition,  [60,-1]);
    
    my $zp_text = $self->{zp_text} = Wx::StaticText->new($self, -1, "Z:",wxDefaultPosition,[15,-1]);
    my $zp_field = $self->{zp_field} = Wx::TextCtrl->new($self, -1, "",wxDefaultPosition,  [60,-1]);
    
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
    my $settings_sizer_sttings = Wx::FlexGridSizer->new( 6, 2, 5, 5);
    my $settings_sizer_positions = Wx::FlexGridSizer->new( 5, 7, 5, 5);
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
    
    
    $settings_sizer_positions->Add($self->{componentsize_text}, 1,wxTOP, 0);
    $settings_sizer_positions->Add($self->{xs_text}, 1,wxTOP, 0);
    $settings_sizer_positions->Add($self->{xs_field}, 1,wxTOP, 0);
    $settings_sizer_positions->Add($self->{ys_text}, 1,wxTOP, 0);
    $settings_sizer_positions->Add($self->{ys_field}, 1,wxTOP, 0);
    $settings_sizer_positions->Add($self->{zs_text}, 1,wxTOP, 0);
    $settings_sizer_positions->Add($self->{zs_field}, 1,wxTOP, 0);
    
    $settings_sizer_positions->Add($self->{componentpos_text}, 1,wxTOP, 0);
    $settings_sizer_positions->Add($self->{xp_text}, 1,wxTOP, 0);
    $settings_sizer_positions->Add($self->{xp_field}, 1,wxTOP, 0);
    $settings_sizer_positions->Add($self->{yp_text}, 1,wxTOP, 0);
    $settings_sizer_positions->Add($self->{yp_field}, 1,wxTOP, 0);
    $settings_sizer_positions->Add($self->{zp_text}, 1,wxTOP, 0); 
    $settings_sizer_positions->Add($self->{zp_field}, 1,wxTOP, 0);    
    
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
            
            # convert scene volume to model object volume
            $self->reload_tree($canvas->volume_idx($volume_idx));
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
# Commet     : 
#######################################################################
sub createDefaultConfig {
    my $self = shift;
    my ($configfile) = @_;
    $self->{config}->{_}{show_parts_on_higher_layers} = 0;
    $self->{config}->{_}{move_step} = 0.1;
    $self->{config}->{_}{footprint_extruder} = 0;
    $self->{config}->{_}{part_extruder} = 0;
    
    $self->{config}->{offset}{chip_x_offset} = 0;
    $self->{config}->{offset}{chip_y_offset} = 0;
    $self->{config}->{offset}{chip_z_offset} = 0;
    
    $self->{config}->{chip_height}{default} = 1;
    Slic3r::Config->write_ini($configfile, $self->{config});
}

#######################################################################
# Purpose    : Reloads canvas when necesary
# Parameters : none
# Returns    : none
# Commet     : 
#######################################################################
sub sliderMoved {
    my $self = shift;
    my $height =  $self->{layers_z}[$self->{slider}->GetValue];
    my $changed = 0;
    
    for my $part (@{$self->{schematic}->{partlist}}) {
        if (($part->{height} && 
            ($part->{shown} == 0 && $part->{position}[2]-$part->{height} <= $height) ||
            ($part->{shown} == 1 && $part->{position}[2]-$part->{height} > $height))) {
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
# Commet     :
#######################################################################
sub reload_print {
    my ($self) = @_;
    $self->canvas->reset_objects;
    $self->_loaded(0);
    for my $part (@{$self->{schematic}->{partlist}}) {
        $part->{shown} = 0;
    }
    $self->load_print;
}

#######################################################################
# Purpose    : loads the print and the objects on the canvas
# Parameters : none
# Returns    : undef if not loaded
# Commet     : First loads Print, second footprints and third parts
#######################################################################
sub load_print {
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
    
    # load objects
    if ($self->IsShown) {
        # load skirt and brim
        $self->canvas->load_print_toolpaths($self->print);
        
        foreach my $object (@{$self->print->objects}) {
            $self->canvas->load_print_object_toolpaths($object);
            
        }
        
        my $height =  $self->{layers_z}[$self->{slider}->GetValue];
                
        my $model_object = $self->{model_object};
        for my $volume_idx (0..$#{$model_object->volumes}) {
            my $volume = $model_object->volumes->[$volume_idx];
            my $part = $self->findPartByVolume($volume);
            if ($part && ($part->{position}[2]-$part->{height} <= $height || $self->{config}->{_}{show_parts_on_higher_layers})) {
                $self->canvas->load_object($model_object,undef,[0],[$volume_idx]);
                $part->{shown} = 1;
            }
        }
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
# Commet     : For mouse placement, also calculates the offset
#######################################################################
sub placePart {
    my $self = shift;
    my ($part, $x, $y, $z) = @_;
    my @offset = @{$self->{schematic}->{root_offset}};
    $part->setPosition($x, $y, $z);
    ($x,$y,$z) = $part->transformWorldtoObject(0,@offset);
    $part->setPosition($x, $y, $z);
    $self->displayPart($part);
    $self->reload_tree($self->findVolumeId($part->{volume}));
}

#######################################################################
# Purpose    : Displays a Part and its footprint on the canvas
# Parameters : part to display
# Returns    : none
# Commet     : When the part already exists on canvas it will be deleted
#######################################################################
sub displayPart {
    my $self = shift;
    my ($part) = @_;
    if ((defined($part->{position}[0]) && !($part->{position}[0] eq "")) &&
        (defined($part->{position}[1]) && !($part->{position}[1] eq "")) &&
        (defined($part->{position}[2]) && !($part->{position}[2] eq ""))) {
        if (!$part->{height}) {
            $part->{height} = $self->get_layer_thickness($part->{position}[2]);
        }
        if ($part->{volume}) {
            my ($x, $y, $z) = @{$part->{position}};
            my ($xr, $yr, $zr) = @{$part->{rotation}};
            $self->removePart($part);
            @{$part->{position}} = ($x, $y, $z);
            @{$part->{rotation}} = ($xr, $yr, $zr);
        }
        my $footprint_model = $part->getFootprintModel($self->{model_object}->instances->[0]->rotation);
            
        foreach my $object (@{$footprint_model->objects}) {
            foreach my $volume (@{$object->volumes}) {
                my $new_volume = $self->{model_object}->add_volume($volume);
                $new_volume->set_modifier(0);
                $new_volume->set_name($part->{name}."-Footprint");
                $new_volume->set_material_id(0);
                
                # set a default extruder value, since user can't add it manually
                $new_volume->config->set_ifndef('extruder', $self->{config}->{_}{footprint_extruder});
                
                $part->{volume} = $new_volume;
            }
        }
        
        my $chip_model = $part->getPartModel($self->{config}, $self->{model_object}->instances->[0]->rotation);
            
        foreach my $object (@{$chip_model->objects}) {
            foreach my $volume (@{$object->volumes}) {
                my $new_volume = $self->{model_object}->add_volume($volume);
                $new_volume->set_modifier(0);
                $new_volume->set_name($part->{name}."-Component");
                $new_volume->set_material_id(0);
                
                # set a default extruder value, since user can't add it manually
                $new_volume->config->set_ifndef('extruder', $self->{config}->{_}{part_extruder});
                
                $part->{chipVolume} = $new_volume;
            }
        }
    }
}

#######################################################################
# Purpose    : Removes a part and its footprint form canvas
# Parameters : part or volume_id
# Returns    : none
# Commet     : 
#######################################################################
sub removePart {
    my $self = shift;
    my ($reference) = @_;
    my $part;
    my $volumeId;
    
    if (blessed($reference) eq "Slic3r::Electronics::ElectronicPart") {
        $volumeId = $self->findVolumeId($reference->{volume});
        $part = $reference;
    } else {
        $volumeId = $reference;
        $part = $self->findPartByVolume($self->{model_object}->volumes->[$reference]);
    }
    if (defined($volumeId)){
        $self->{model_object}->delete_volume($volumeId);
    }
    my $chipVolumeId = $self->findVolumeId($part->{chipVolume});
    if (defined($chipVolumeId)){
        $self->{model_object}->delete_volume($chipVolumeId);
    }
    $part->removePart;
    $self->reload_tree;
}

#######################################################################
# Purpose    : Reloads the model tree
# Parameters : currently selected volume
# Returns    : none
# Commet     : 
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
    foreach my $volume_id (0..$#{$object->volumes}) {
        my $volume = $object->volumes->[$volume_id];
        
        my $icon = $volume->modifier ? ICON_MODIFIERMESH : ICON_SOLIDMESH;
        my $part = $self->findPartByVolume($volume);
        $icon = $part ? ICON_PCB : ICON_SOLIDMESH;
        my $itemId = $tree->AppendItem($rootId, $volume->name || $volume_id, $icon);
        if ($volume_id == $selected_volume_idx) {
            $selectedId = $itemId;
        }
        $tree->SetPlData($itemId, {
            type        => 'volume',
            volume_id   => $volume_id,
            volume      => $volume,
        });
    }
    if (defined $self->{schematic}) {
        my $length = @{$self->{schematic}->{partlist}};
        if ($length > 0) {
            my $eIcon = ICON_PCB;
            my $eItemId = $tree->AppendItem($rootId, "unplaced");
            $tree->SetPlData($eItemId, {
                type        => 'unplaced',
                volume_id   => 0,
            });
            foreach my $part (@{$self->{schematic}->{partlist}}) {
                if (!$part->{volume}) {
                    my $ItemId = $tree->AppendItem($eItemId, $part->{name}, $eIcon);
                    $tree->SetPlData($ItemId, {
                        type        => 'part',
                        part        => $part,
                    });
                }
            }
        }
    }
    $tree->ExpandAll;
    
    $self->{tree}->SelectItem($selectedId);
}

#######################################################################
# Purpose    : Gets the selected volume form the model tree
# Parameters : none
# Returns    : volumeid or undef
# Commet     :
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
# Commet     :
#######################################################################
sub selection_changed {
    my ($self) = @_;
    my $selection = $self->get_selection;
    my $part = ();
    if ($selection->{type} eq 'part') {
        $part = $selection->{part};
    }
    if ($selection->{type} eq 'volume' && $selection->{volume}) {
        my $volume = $selection->{volume};
        $part = $self->findPartByVolume($volume);
    }
    if ($part) {
        $self->showPartInfo($part);
    }
    else {
        $self->clearPartInfo;
    }
}

#######################################################################
# Purpose    : Gets the current z position of the canvas
# Parameters : none
# Returns    : z position
# Commet     :
#######################################################################
sub get_z {
    my ($self) = @_;
    return $self->{layers_z}[$self->{slider}->GetValue];
}

#######################################################################
# Purpose    : Sets the z position on the canvas
# Parameters : z position
# Returns    : undef if canvas is not active
# Commet     :
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
# Commet     :
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
# Commet     :
#######################################################################
sub showPartInfo {
    my $self = shift;
    my ($part) = @_;
    $self->clearPartInfo;
    $self->{name_field}->SetValue($part->{name}) if defined($part->{name});
    $self->{library_field}->SetValue($part->{library})if defined($part->{library});
    $self->{deviceset_field}->SetValue($part->{deviceset})if defined($part->{deviceset});
    $self->{device_field}->SetValue($part->{device})if defined($part->{device});
    $self->{package_field}->SetValue($part->{package})if defined($part->{package});
    $self->{height_field}->SetValue($part->{height})if defined($part->{height});
    $self->{x_field}->SetValue($part->{position}[0]) if (defined($part->{position}[0]));
    $self->{y_field}->SetValue($part->{position}[1]) if (defined($part->{position}[1]));
    $self->{z_field}->SetValue($part->{position}[2]) if (defined($part->{position}[2]));
    $self->{xr_field}->SetValue($part->{rotation}[0]) if (defined($part->{rotation}[0]));
    $self->{yr_field}->SetValue($part->{rotation}[1]) if (defined($part->{rotation}[1]));
    $self->{zr_field}->SetValue($part->{rotation}[2]) if (defined($part->{rotation}[2]));
    $self->{xs_field}->SetValue($part->{componentsize}[0]) if (defined($part->{componentsize}[0]));
    $self->{ys_field}->SetValue($part->{componentsize}[1]) if (defined($part->{componentsize}[1]));
    $self->{zs_field}->SetValue($part->{componentsize}[2]) if (defined($part->{componentsize}[2]));
    $self->{xp_field}->SetValue($part->{componentpos}[0]) if (defined($part->{componentpos}[0]));
    $self->{yp_field}->SetValue($part->{componentpos}[1]) if (defined($part->{componentpos}[1]));
    $self->{zp_field}->SetValue($part->{componentpos}[2]) if (defined($part->{componentpos}[2]));
}

#######################################################################
# Purpose    : Clears the part info
# Parameters : none
# Returns    : none
# Commet     :
#######################################################################
sub clearPartInfo {
    my $self = shift;
    $self->{name_field}->SetValue("");
    $self->{library_field}->SetValue("");
    $self->{deviceset_field}->SetValue("");
    $self->{device_field}->SetValue("");
    $self->{package_field}->SetValue("");
    $self->{height_field}->SetValue("");
    $self->{x_field}->SetValue("");
    $self->{y_field}->SetValue("");
    $self->{z_field}->SetValue("");
    $self->{xr_field}->SetValue("");
    $self->{yr_field}->SetValue("");
    $self->{zr_field}->SetValue("");
    $self->{xs_field}->SetValue("");
    $self->{ys_field}->SetValue("");
    $self->{zs_field}->SetValue("");
    $self->{xp_field}->SetValue("");
    $self->{yp_field}->SetValue("");
    $self->{zp_field}->SetValue("");
}

#######################################################################
# Purpose    : Saves the partinfo of the displayed part
# Parameters : part to save
# Returns    : none
# Commet     :
#######################################################################
sub savePartInfo {
    my $self = shift;
    my ($part) = @_;
    $part->{name} = $self->{name_field}->GetValue;
    $part->{library} = $self->{library_field}->GetValue;
    $part->{deviceset} = $self->{deviceset_field}->GetValue;
    $part->{device} = $self->{device_field}->GetValue;
    $part->{package} = $self->{package_field}->GetValue;
    $part->{height} = $self->{height_field}->GetValue;
    @{$part->{position}} = ($self->{x_field}->GetValue, $self->{y_field}->GetValue, $self->{z_field}->GetValue) if (!($self->{x_field}->GetValue eq "") && !($self->{y_field}->GetValue eq "") && !($self->{z_field}->GetValue eq ""));
    @{$part->{rotation}} = ($self->{xr_field}->GetValue, $self->{yr_field}->GetValue, $self->{zr_field}->GetValue) if (!($self->{xr_field}->GetValue eq "") && !($self->{yr_field}->GetValue eq "") && !($self->{zr_field}->GetValue eq ""));
    @{$part->{componentsize}} = ($self->{xs_field}->GetValue, $self->{ys_field}->GetValue, $self->{zs_field}->GetValue) if (!($self->{xs_field}->GetValue eq "") && !($self->{ys_field}->GetValue eq "") && !($self->{zs_field}->GetValue eq ""));
    @{$part->{componentpos}} = ($self->{xp_field}->GetValue, $self->{yp_field}->GetValue, $self->{zp_field}->GetValue) if (!($self->{xp_field}->GetValue eq "") && !($self->{yp_field}->GetValue eq "") && !($self->{zp_field}->GetValue eq ""));
    $self->displayPart($part);
        
}

#######################################################################
# Purpose    : Event for load button
# Parameters : $file to load
# Returns    : none
# Commet     : calls the method to read the file
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
    for my $part (@{$self->{schematic}->{partlist}}) {
        $self->removePart($part);
    }
    Slic3r::Electronics::Electronics->readFile($file,$self->{schematic}, $self->{config});
    for my $part (@{$self->{schematic}->{partlist}}) {
        $self->displayPart($part);
    }

    $self->reload_tree;
}

#######################################################################
# Purpose    : Event for place button
# Parameters : none
# Returns    : none
# Commet     : sets the place variable to the current selection
#######################################################################
sub placeButtonPressed {
    my $self = shift;
    $self->{place} = $self->get_selection;
}

#######################################################################
# Purpose    : Returns the current item to place
# Parameters : none
# Returns    : item to place
# Commet     :
#######################################################################
sub get_place {
    my $self = shift;
    return $self->{place};
}

#######################################################################
# Purpose    : Sets the item to place
# Parameters : item to place
# Returns    : none
# Commet     :
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
# Commet     : calls the remove function
#######################################################################
sub removeButtonPressed {
    my $self = shift;
    my $selection = $self->get_selection;
    if ($selection->{type} eq 'volume') {
        my $part = $self->findPartByVolume($selection->{volume});
        $self->removePart($part) if defined($part);
    }
}

#######################################################################
# Purpose    : Event for save part button
# Parameters : none
# Returns    : none
# Commet     : saves the partinfo
#######################################################################
sub savePartButtonPressed {
    my $self = shift;
    my $selection = $self->get_selection;
    my $part = ();
    if ($selection->{type} eq 'part') {
        $part = $selection->{part};
    }
    if ($selection->{type} eq 'volume' && $selection->{volume}) {
        my $volume = $selection->{volume};
        $part = $self->findPartByVolume($volume);
    }
    if ($part) {
        $self->savePartInfo($part);
        $self->reload_tree($self->findVolumeId($part->{volume}));
    }
}

#######################################################################
# Purpose    : Event for save button
# Parameters : none
# Returns    : none
# Commet     : Calls Slic3r::Electronics::Electronics->writeFile
#######################################################################
sub saveButtonPressed {
    my $self = shift;
    my ($base,$path,$type) = fileparse($self->{schematic}->{filename},('.sch','.SCH','3de','.3DE'));
    if (Slic3r::Electronics::Electronics->writeFile($self->{schematic},$self->{config})) {
        Wx::MessageBox('File saved as '.$base.'.3de','Saved', Wx::wxICON_INFORMATION | Wx::wxOK,undef)
    } else {
        Wx::MessageBox('Saving failed','Failed',Wx::wxICON_ERROR | Wx::wxOK,undef)
    }
}

#######################################################################
# Purpose    : MovesPart with Buttons
# Parameters : x, y and z coordinates
# Returns    : none
# Commet     : moves Z by layer thickness
#######################################################################
sub movePart {
    my $self = shift;
    my ($x,$y,$z) = @_;
    my $selected = $self->get_selection;
    my $part = $self->findPartByVolume($selected->{volume});
    if ($part) {
        if ($z != 0) {
            $part->{position}[2] += $self->get_layer_thickness($part->{position}[2])*$z;
        }
        $part->{position}[0] += $x;
        $part->{position}[1] += $y;
        $self->showPartInfo($part);
        $self->displayPart($part);
        $self->reload_tree($self->findVolumeId($part->{volume}));
    }
}

#######################################################################
# Purpose    : Returns the part to a given volume
# Parameters : volume to find
# Returns    : part or undef
# Commet     : compares volumes with Data::Dumper
#######################################################################
sub findPartByVolume {
    my $self = shift;
    my ($volume) = @_;
    if (defined $self->{schematic}) {
        for my $part (@{$self->{schematic}->{partlist}}) {
            if (Dumper($part->{volume}) eq Dumper($volume) || Dumper($part->{chipVolume}) eq Dumper($volume)) {
                return $part;  
            } 
        }
    }
    return;
}

#######################################################################
# Purpose    : Returns a volumeID to a given volume
# Parameters : volume to find
# Returns    : volumeid
# Commet     : compares volumes with Data::Dumper
#######################################################################
sub findVolumeId {
    my $self = shift;
    my ($volume) = @_;
    my $object  = $self->{model_object};
    for my $volume_id (0..$#{$object->volumes}) {
        if (Dumper($object->volumes->[$volume_id]) eq Dumper($volume)) {
            return $volume_id;  
        }            
    }
    return undef;
}

1;
