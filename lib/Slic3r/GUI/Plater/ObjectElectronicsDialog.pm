package Slic3r::GUI::Plater::ObjectElectronicsDialog;
use strict;
use warnings;
use utf8;

use Slic3r::Electronics::Filereaders::Eagle;
use Wx qw(:dialog :id :misc :sizer :systemsettings :notebook wxTAB_TRAVERSAL);
use Wx::Event qw(EVT_BUTTON EVT_CLOSE);
use base 'Wx::Dialog';

#######################################################################
# Purpose    : Creates a new Frame for 3DElectronics
# Parameters : name, object model, and source filename
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
    	model_object => $params{model_object}),
    "Electronics");

    my $sizer = Wx::BoxSizer->new(wxVERTICAL);
    $sizer->Add($self->{tabpanel}, 1, wxEXPAND | wxTOP | wxLEFT | wxRIGHT, 10);
    
    $self->SetSizer($sizer);
    $self->SetMinSize($self->GetSize);
    
    # catch key events to interrupt actions on the canvas with ESC key
    Wx::Event::EVT_CHAR_HOOK($self, sub {
    	my ($self, $e) = @_;
    	if($e->GetKeyCode == Wx::WXK_ESCAPE) {
    		$self->{parts}->{canvas}->cancel_action;
    	}
    	$e->Skip();
    });
    
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
use Slic3r::ElectronicPart qw(:PlacingMethods :ConnectionMethods);
use Slic3r::GUI::Electronics3DScene;
use Slic3r::Config;
use File::Basename qw(basename);
use Wx qw(:misc :sizer :slider :treectrl :button :filedialog :propgrid wxTAB_TRAVERSAL wxSUNKEN_BORDER wxBITMAP_TYPE_PNG wxFD_OPEN wxFD_FILE_MUST_EXIST wxID_OK
    wxTheApp wxTE_READONLY);
use Wx::Event qw(EVT_BUTTON EVT_TREE_ITEM_COLLAPSING EVT_TREE_ITEM_ACTIVATED EVT_TREE_SEL_CHANGED EVT_PG_CHANGED EVT_SLIDER EVT_MOUSE_EVENTS);
use Wx::PropertyGrid;
use base qw(Wx::Panel Class::Accessor);
use Scalar::Util qw(blessed);
use File::Basename;
use Data::Dumper;

__PACKAGE__->mk_accessors(qw(print enabled _loaded canvas slider));

use constant {
	ICON_OBJECT        => 0,
	ICON_SOLIDMESH     => 1,
	ICON_MODIFIERMESH  => 2,
	ICON_PCB           => 3,
};

use constant {
	PROPERTY_PART		=> 0,
	PROPERTY_WAYPOINT	=> 1,
	PROPERTY_NET		=> 2,
};

#######################################################################
# Purpose    : Creates a Panel for 3DElectronics
# Parameters : model_object and source filename to edit
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
    $self->{schematic} = $print->objects->[$self->{obj_idx}]->schematic;
    my $place = $self->{place} = 0;
    $self->{model_object}->update_bounding_box;
    my $configfile ||= Slic3r::decode_path(Wx::StandardPaths::Get->GetUserDataDir . "/electronics/electronics.ini");
    my $config = $self->{config};
    if (-f $configfile) {
        $self->{config} = eval { Slic3r::Config->read_ini($configfile) };
    } else {
        $self->createDefaultConfig($configfile);
    }
    
    # Lookup-tables to match selected object IDs from canvas to actual object
	$self->{rubberband_lookup} = ();
	$self->{part_lookup} = ();
	$self->{netPoint_lookup} = ();
	
    
    # upper buttons
    my $btn_load_netlist = $self->{btn_load_netlist} = Wx::Button->new($self, -1, "Load netlist", wxDefaultPosition, wxDefaultSize, wxBU_LEFT);
    my $btn_save_netlist = $self->{btn_save_netlist} = Wx::Button->new($self, -1, "Save netlist", wxDefaultPosition, wxDefaultSize, wxBU_LEFT);
    
    # upper buttons sizer
    my $buttons_sizer = Wx::FlexGridSizer->new( 1, 3, 5, 5);
    $buttons_sizer->Add($btn_load_netlist, 0);
    $buttons_sizer->Add($btn_save_netlist, 0);
    $btn_load_netlist->SetFont($Slic3r::GUI::small_font);    
    $btn_save_netlist->SetFont($Slic3r::GUI::small_font);
    

    # create TreeCtrl
    my $tree = $self->{tree} = Wx::TreeCtrl->new($self, -1, wxDefaultPosition, [350,-1], 
        wxTR_NO_BUTTONS | wxSUNKEN_BORDER | wxTR_HAS_VARIABLE_ROW_HEIGHT
        | wxTR_SINGLE | wxTR_NO_BUTTONS | wxEXPAND);
    {
        $self->{tree_icons} = Wx::ImageList->new(16, 16, 1);
        $tree->AssignImageList($self->{tree_icons});
        $self->{tree_icons}->Add(Wx::Bitmap->new($Slic3r::var->("brick.png"), wxBITMAP_TYPE_PNG));     # ICON_OBJECT
        $self->{tree_icons}->Add(Wx::Bitmap->new($Slic3r::var->("package.png"), wxBITMAP_TYPE_PNG));   # ICON_SOLIDMESH
        $self->{tree_icons}->Add(Wx::Bitmap->new($Slic3r::var->("plugin.png"), wxBITMAP_TYPE_PNG));    # ICON_MODIFIERMESH
        $self->{tree_icons}->Add(Wx::Bitmap->new($Slic3r::var->("PCB-icon.png"), wxBITMAP_TYPE_PNG));  # ICON_PCB
        
        my $rootId = $tree->AddRoot("Object", ICON_OBJECT);
        $tree->SetPlData($rootId, { type => 'object' });
    }
    
    
    # create PropertyGrid as central represation for part / waypoint / net properties
    my $propgrid = $self->{propgrid} = Wx::PropertyGrid->new($self, -1, wxDefaultPosition, [350,-1], 
        wxPG_SPLITTER_AUTO_CENTER | wxPG_DEFAULT_STYLE);
    
    $self->{property_selected_object} = -1;


    # part settings fields    
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
    my $settings_sizer_positions = Wx::FlexGridSizer->new( 3, 7, 5, 5);
    
    $settings_sizer_main->Add($settings_sizer_main_grid, 0,wxTOP, 0);
    
    $settings_sizer_main_grid->Add($settings_sizer_positions, 0,wxTOP, 0);
    
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
    
    
    # lower buttons 
    #my $btn_save_netlist = $self->{btn_save_netlist} = Wx::Button->new($self, -1, "Save netlist", wxDefaultPosition, wxDefaultSize, wxBU_LEFT);
    
    # lower buttons sizer
    #my $buttons_sizer_bottom = Wx::FlexGridSizer->new( 1, 3, 5, 5);
    #$buttons_sizer_bottom->Add($btn_save_netlist, 0);
    #$btn_save_netlist->SetFont($Slic3r::GUI::small_font);
    
    # left pane with tree
    my $left_sizer = Wx::BoxSizer->new(wxVERTICAL);
    $left_sizer->Add($buttons_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 5);
    $left_sizer->Add($tree, 1, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 5);
    $left_sizer->Add($propgrid, 1, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 5);
    $left_sizer->Add($settings_sizer_main, 0, wxEXPAND | wxALL| wxRIGHT | wxTOP, 5);
    
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
    
    $self->{is_zoomed} = 0;
    
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
        $canvas = $self->{canvas} = Slic3r::GUI::Electronics3DScene->new($self, $self->{schematic});
        $canvas->enable_picking(1);
        $canvas->select_by('volume');
        
        $canvas->on_select(sub {
            my ($volume_idx) = @_;
            return if($volume_idx < 0);
            
            # object is a part
            my $part = $self->{part_lookup}[$volume_idx];
            if(defined $part) {
            	$self->reload_tree($part->getPartID);	
            }
            
            # object is a rubberband
            my $rubberband = $self->{rubberband_lookup}[$volume_idx];
            if(defined $rubberband) {
            	#
            }
            
            # object is a waypoint
            my $waypoint = $self->{netPoint_lookup}[$volume_idx];
            if(defined $waypoint) {
            	# populate property grid
			    $self->{propgrid}->Clear;
			    my $prop;
			    # read only info values
			    
			    #$prop = $self->{propgrid}->Append(new Wx::StringProperty("Waypoint type", "Waypoint type", $wayopint->getName()));
			    #$prop->Enable(0);
			    
			    my $position = $waypoint->getPoint();
			    $self->{propgrid}->Append(new Wx::PropertyCategory("Position"));
			    $prop = $self->{propgrid}->Append(new Wx::FloatProperty("X", "X", $position->x));
			    $prop->Enable(0) if(!$waypoint->isWaypointType);
			    $prop = $self->{propgrid}->Append(new Wx::FloatProperty("Y", "Y", $position->y));
			    $prop->Enable(0) if(!$waypoint->isWaypointType);
			    $prop = $self->{propgrid}->Append(new Wx::FloatProperty("Z", "Z", $position->z));
			    $prop->Enable(0) if(!$waypoint->isWaypointType);
			    
			    $self->{property_selected_type} = PROPERTY_WAYPOINT;
    			$self->{property_selected_object} = $waypoint;
            }
        });
        
        $canvas->on_double_click(sub {
            my ($volume_idx) = @_;
            return if($volume_idx < 0);
            
            # object is a part
            
            # object is a rubberband -> split this rubberband
            my $rubberband = $self->{rubberband_lookup}[$volume_idx];
            if(defined $rubberband) {
            	$self->{plater}->stop_background_process;
            	my $mousepoint = $self->{canvas}->get_mouse_pos_3d_obj;
            	if(!$rubberband->isWired) { # if rubberband is not wired, start routing from closest point
            		$rubberband->selectNearest($mousepoint);
            	}
            	$canvas->rubberband_splitting($rubberband);
            }
            
            # object is a point -> create wire starting from this point
            my $netPoint = $self->{netPoint_lookup}[$volume_idx];
            if(defined $netPoint) {
            	$self->{plater}->stop_background_process;
            	my $mousepoint = $self->{canvas}->get_mouse_pos_3d_obj;
            	$canvas->point_splitting($netPoint);
            }
        });
        
        $canvas->on_right_double_click(sub {
            my ($volume_idx) = @_;
            return if($volume_idx < 0);
            
            # object is a part -> remove part from canvas
            my $part = $self->{part_lookup}[$volume_idx];
            if(defined $part) {
            	$self->{plater}->stop_background_process;
    			$part->setPlaced(0);
    			$part->resetPosition;
    			$self->reload_tree($part->getPartID);
    			$self->triggerSlicing;
            }
            
            # object is a rubberband -> delete this wire
            my $rubberband = $self->{rubberband_lookup}[$volume_idx];
            if(defined $rubberband) {
            	$self->{plater}->stop_background_process;
            	if($self->{schematic}->removeWire($rubberband->getID)) {
            		$self->reload_print;
            		$self->triggerSlicing;
            	}
            }
            
            # object is a netPoint -> delete this point
            my $netPoint = $self->{netPoint_lookup}[$volume_idx];
            if(defined $netPoint) {
            	$self->{plater}->stop_background_process;
            	if($self->{schematic}->removeNetPoint($netPoint)) {
            		$self->reload_print;
            		$self->triggerSlicing;
            	}
            }
        });
        # callback for placing a part from the list on the canvas
        $canvas->on_place_part(sub {
            my ($part, $pos) = @_;
            $self->placePart($part, $pos);
        });
        
        $canvas->on_rubberband_split(sub {
            my ($rubberband, $pos) = @_;
            $self->{plater}->stop_background_process;
            # translate to center of layer
            $pos->translate(0, 0, $self->get_layer_thickness($pos->z)/2);
			$self->{schematic}->splitWire($rubberband, $pos);      
			$self->reload_print;
			$self->triggerSlicing;
        });
        
        $canvas->on_waypoint_split(sub {
            my ($netPoint, $pos) = @_;
            $self->{plater}->stop_background_process;
            # translate to center of layer
            $pos->translate(0, 0, $self->get_layer_thickness($pos->z)/2);
			$self->{schematic}->addWire($netPoint, $pos);      
			$self->reload_print;
			$self->triggerSlicing;
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
    
    # set part to be selected by left-clicking on the canvas
    EVT_TREE_ITEM_ACTIVATED($self, $tree, sub {
        my ($self, $event) = @_;
        my $selection = $self->get_tree_selection;
        if ($selection->{type} eq 'part') {
        	my $part = $selection->{part};
        	if(!$part->isPlaced) {
        		my $mesh = $part->getMesh();
        		$self->{canvas}->place_electronic_part($part);
        	}
        }
    });
    
    EVT_TREE_SEL_CHANGED($self, $tree, sub {
        my ($self, $event) = @_;
        return if $self->{disable_tree_sel_changed_event};
        $self->tree_selection_changed;
    });
    
    # Property grid changed
    EVT_PG_CHANGED($self, $propgrid, sub {
        my ($self, $event) = @_;
        $self->property_selection_changed;
    });
    
    EVT_BUTTON($self, $self->{btn_load_netlist}, sub { 
        $self->loadButtonPressed;
    });
    
    EVT_BUTTON($self, $self->{btn_remove_part}, sub { 
        $self->removeButtonPressed; 
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
    
    #my $partlist = $self->{schematic}->getPartlist();
    #for my $part (@{$partlist}) {
    #	$part->setVisibility(0);
    #}
    
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
    
    # reset lookup array
    $self->{part_lookup} = ();
    $self->{rubberband_lookup} = ();
    $self->{netPoint_lookup} = ();
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
        		#if ($part->getPosition()->z-$part->getFootprintHeight() <= $height || $self->{config}->{_}{show_parts_on_higher_layers}) {
        		#	$part->setVisibility(1);
        			my $mesh = $part->getMesh();
        			my $offset = $self->{model_object}->_bounding_box->center;
        			$mesh->translate($offset->x, $offset->y, 0);
        			my $object_id = $self->canvas->load_electronic_part($mesh);
        			# lookup table
        			$self->{part_lookup}[$object_id] = $part;
        		#}else{
        		#	$part->setVisibility(0);
        		#}
        	}
        }
        # Display rubber-banding
        my $rubberBands = $self->{schematic}->getRubberBands();
	    foreach my $rubberBand (@{$rubberBands}) {
	    	if($rubberBand->isWired()) {
	    		my $object_id = $self->canvas->add_rubberband($rubberBand->a, $rubberBand->b, 0.3, [0.58, 0.83, 1.0, 0.9]);
	    		# lookup table
	    		$self->{rubberband_lookup}[$object_id] = $rubberBand;
	    	}else{
	    		my $object_id = $self->canvas->add_rubberband($rubberBand->a, $rubberBand->b, 0.3, [0.2, 0.2, 0.2, 0.9]);
	    		# lookup table
	    		$self->{rubberband_lookup}[$object_id] = $rubberBand;
	    	}
	    }
	    # Display wire points
	    my $netPoints = $self->{schematic}->getNetPoints;
	    foreach my $netPoint (@{$netPoints}) {
	    	my $object_id = $self->canvas->add_wire_point($netPoint->getPoint, [0.8, 0.1, 0.1, 0.9]);
	    	# lookup table
	    	$self->{netPoint_lookup}[$object_id] = $netPoint;
	    }
	    #$self->canvas->add_wire_point(Slic3r::Pointf3->new(0, 0, 0), [0.8, 0.1, 0.1, 0.9]);
        $self->set_z($height) if $self->enabled;
        
        if (!$self->{is_zoomed}) {
            $self->canvas->zoom_to_volumes;
            $self->{is_zoomed} = 1;
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
    my ($part, $pos) = @_;
    
    $self->{plater}->stop_background_process;
    
    # round values from canvas
    my $x = int($pos->x*1000)/1000.0;
    my $y = int($pos->y*1000)/1000.0;
    my $z = int($pos->z*1000)/1000.0;
    $part->setPosition($x, $y, $z);
    $part->setFootprintHeight($self->get_layer_thickness($z));
    $part->setPlaced(1);
    $part->setVisibility(1);
    $self->reload_tree($part->getPartID());
    
    # trigger slicing steps to update modifications;
    $self->triggerSlicing;
    
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
        if ($#{$partlist} >= 0) {
        	
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
}

#######################################################################
# Purpose    : Gets the selected volume form the model tree
# Parameters : none
# Returns    : volumeid or undef
# Comment     :
#######################################################################
sub get_tree_selection {
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
sub tree_selection_changed {
    my ($self) = @_;
    my $selection = $self->get_tree_selection;
    if ($selection->{type} eq 'part') {
    	my $part = $selection->{part};
    	# jump to corresponding layer
    	if($part->isPlaced) {
    		if($self->get_z < $part->getPosition->z || $self->get_z > $part->getPosition->z + $part->getPartHeight) {
    			
    			for my $i (0 .. $#{$self->{layers_z}}) {
    				if($self->{layers_z}[$i] >= $part->getPosition->z) {
    					$self->{slider}->SetValue($i);
    					$self->sliderMoved;
    					last;
    				}
    			}
    		}
    	}
    	$self->loadPartProperties($part);
    	$self->{property_selected_type} = PROPERTY_PART;
    	$self->{property_selected_object} = $part;
    }else {
        $self->{propgrid}->Clear;
    }
}


# saves updated values from the propertyGrid
sub property_selection_changed {
    my ($self) = @_;
    
    # current properties are of type...
    if($self->{property_selected_type} == PROPERTY_PART) {
        $self->savePartProperties($self->{property_selected_object});
        $self->reload_tree($self->{property_selected_object}->getPartID());
    }
    
    if($self->{property_selected_type} == PROPERTY_WAYPOINT) {
        my $x = $self->{propgrid}->GetPropertyValue("Position.X")->GetDouble;
        my $y = $self->{propgrid}->GetPropertyValue("Position.Y")->GetDouble;
        my $z = $self->{propgrid}->GetPropertyValue("Position.Z")->GetDouble;
        $self->{property_selected_object}->setPosition($x, $y, $z);
    }
    
    $self->{schematic}->updateRubberBands();
    
    # trigger slicing steps to update modifications;
    $self->{print}->objects->[$self->{obj_idx}]->invalidate_step(STEP_SLICE);
    # calls schedule to update the toolpath to give the user an opportunity for multiple position updates
    $self->{plater}->schedule_background_process;
}


#######################################################################
# Purpose    : Shows the part info in the GUI
# Parameters : part to display
# Returns    : none
# Comment     :
#######################################################################
sub loadPartProperties {
    my $self = shift;
    my ($part) = @_;
    
    # populate property grid
    $self->{propgrid}->Clear;
    my $prop;
    # read only info values
    $prop = $self->{propgrid}->Append(new Wx::StringProperty("Part name", "Part name", $part->getName()));
    $prop->Enable(0);
    $prop = $self->{propgrid}->Append(new Wx::StringProperty("Library", "Library", $part->getLibrary()));
    $prop->Enable(0);
    $prop = $self->{propgrid}->Append(new Wx::StringProperty("Deviceset", "Deviceset", $part->getDeviceset()));
    $prop->Enable(0);
    $prop = $self->{propgrid}->Append(new Wx::StringProperty("Device", "Device", $part->getDevice()));
    $prop->Enable(0);
    $prop = $self->{propgrid}->Append(new Wx::StringProperty("Package", "Package", $part->getPackage()));
    $prop->Enable(0);
    
    # editable values
    $prop = $self->{propgrid}->Append(new Wx::FloatProperty("Part height", "Part height", $part->getPartHeight()));
    $prop->Enable(0) if(!$part->isPlaced);
    $prop = $self->{propgrid}->Append(new Wx::FloatProperty("Footprint height", "Footprint height", $part->getFootprintHeight()));
    $prop->Enable(0);
    
    # how to place this part?
    my $placingMethod = new Wx::PGChoices();
	$placingMethod->Add("Automatic", PM_AUTOMATIC);
	$placingMethod->Add("Manual", PM_MANUAL);
	$placingMethod->Add("None", PM_NONE);
    
    $prop = $self->{propgrid}->Append(new Wx::EnumProperty("Placing method", "Placing method", $placingMethod, $part->getPlacingMethod));
    $prop->Enable(0) if(!$part->isPlaced);

    # how to connect this part?
    my $connectionMethod = new Wx::PGChoices();
    $connectionMethod->Add("None", CM_NONE);
    $connectionMethod->Add("Layer", CM_LAYER);
    $connectionMethod->Add("Part", CM_PART);

    $prop = $self->{propgrid}->Append(new Wx::EnumProperty("Connection method", "Connection method", $connectionMethod, $part->getConnectionMethod));
    $prop->Enable(0) if(!$part->isPlaced);

	# position    
    my $position = $part->getPosition();
    $self->{propgrid}->Append(new Wx::PropertyCategory("Position"));
    $prop = $self->{propgrid}->Append(new Wx::FloatProperty("X", "X", $position->x));
    $prop->Enable(0) if(!$part->isPlaced);
    $prop = $self->{propgrid}->Append(new Wx::FloatProperty("Y", "Y", $position->y));
    $prop->Enable(0) if(!$part->isPlaced);
    $prop = $self->{propgrid}->Append(new Wx::FloatProperty("Z", "Z", $position->z));
    $prop->Enable(0) if(!$part->isPlaced);
    
    #rotation
    my $rotation = $part->getRotation();
    $prop = $self->{propgrid}->Append(new Wx::PropertyCategory("Rotation"));
    $self->{propgrid}->Append(new Wx::FloatProperty("X", "X", $rotation->x));
    $prop->Enable(0) if(!$part->isPlaced);
    $prop = $self->{propgrid}->Append(new Wx::FloatProperty("Y", "Y", $rotation->y));
    $prop->Enable(0) if(!$part->isPlaced);
    $prop = $self->{propgrid}->Append(new Wx::FloatProperty("Z", "Z", $rotation->z));
    $prop->Enable(0) if(!$part->isPlaced);
}

#######################################################################
# Purpose    : Saves the partinfo of the displayed part
# Parameters : part to save
# Returns    : none
# Comment     :
#######################################################################
sub savePartProperties {
    my $self = shift;
    my ($part) = @_;
    
	$part->setPartHeight($self->{propgrid}->GetPropertyValue("Part height")->GetDouble);
	$part->setPlacingMethod($self->{propgrid}->GetPropertyValue("Placing method")->GetLong);
	$part->setConnectionMethod($self->{propgrid}->GetPropertyValue("Connection method")->GetLong);

	$part->setPosition($self->{propgrid}->GetPropertyValue("Position.X")->GetDouble,
		$self->{propgrid}->GetPropertyValue("Position.Y")->GetDouble,
		$self->{propgrid}->GetPropertyValue("Position.Z")->GetDouble
	);
	
	$part->setRotation($self->{propgrid}->GetPropertyValue("Rotation.X")->GetDouble,
		$self->{propgrid}->GetPropertyValue("Rotation.Y")->GetDouble,
		$self->{propgrid}->GetPropertyValue("Rotation.Z")->GetDouble
	);
}

#######################################################################
# Purpose    : Event for load button
# Parameters : $file to load
# Returns    : none
# Comment    : calls the method to read the file
#######################################################################
sub loadButtonPressed {
    my $self = shift;
    my ($filename) = @_;
    
    if (!$filename) {
        my $dir = $Slic3r::GUI::Settings->{recent}{skein_directory}
           || $Slic3r::GUI::Settings->{recent}{config_directory}
           || '';
        
        my $dlg = Wx::FileDialog->new(
            $self, 
            'Select schematic to load:',
            $dir,
            '',
            &Slic3r::GUI::FILE_WILDCARDS->{sch}, 
            wxFD_OPEN | wxFD_FILE_MUST_EXIST);
        return unless $dlg->ShowModal == wxID_OK;
        $filename = Slic3r::decode_path($dlg->GetPaths);
        $dlg->Destroy;
    }

    my ($base,$path,$type) = fileparse($filename,('.sch','.SCH','3de','.3DE'));
    if ($type eq "sch" || $type eq "SCH" || $type eq ".sch" || $type eq ".SCH") {
        Slic3r::Electronics::Filereaders::Eagle->readFile($filename,$self->{schematic}, $self->{config});
    } elsif ($type eq "3de" || $type eq "3DE" || $type eq ".3de" || $type eq ".3DE") {
    	# read corresponding .sch file first
    	my $parser = XML::LibXML->new();
        my $xmldoc = $parser->parse_file($filename);
        for my $files ($xmldoc->findnodes('/electronics/filename')) {
            print "file to be opened: " . $path . $files->getAttribute('source') . "\n";
            Slic3r::Electronics::Filereaders::Eagle->readFile($path . $files->getAttribute('source'), $self->{schematic}, $self->{config});
        }
    	$self->{schematic}->load3deFile($filename);
    }

    $self->reload_tree;
    $self->triggerSlicing;
}


#######################################################################
# Purpose    : Event for button to save current design state as .3de file
# Parameters : none
# Returns    : none
#######################################################################
sub saveButtonPressed {
    my $self = shift;
    my ($base,$path,$type) = fileparse($self->{schematic}->getFilename,('.sch','.SCH','3de','.3DE'));
    my $savePath = $path . $base . ".3de";
    my $filename = $base . ".3de";
    my $schematicPath = $base . $type;
    
    my $dlg = Wx::FileDialog->new($self, 'Save electronics design file as:', $path,
        $filename, &Slic3r::GUI::FILE_WILDCARDS->{elec}, wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if ($dlg->ShowModal != wxID_OK) {
        $dlg->Destroy;
        return;
    }
    $savePath = Slic3r::decode_path($dlg->GetPaths);
    
    if($self->{schematic}->write3deFile($savePath, $schematicPath)) {
   #     Wx::MessageBox('File saved to ' . $savePath . '.3de','Saved', Wx::wxICON_INFORMATION | Wx::wxOK, undef);
    } else {
        Wx::MessageBox('Saving failed','Failed', Wx::wxICON_ERROR | Wx::wxOK, undef)
    }
}

#######################################################################
# Purpose    : MovesPart with Buttons
# Parameters : x, y and z coordinates
# Returns    : none
# Comment    : moves Z by layer thickness
#######################################################################
sub movePart {
    my $self = shift;
    my ($x,$y,$z) = @_;
    
    my $selection = $self->get_tree_selection;
    if ($selection->{type} eq 'part') {
        my $part = $selection->{part};
        
        $self->{plater}->stop_background_process;
        my $oldPos = $part->getPosition;
        $part->setPosition($oldPos->x + $x, $oldPos->y + $y, $oldPos->z + $self->get_layer_thickness($oldPos->z)*$z);
        
        $self->loadPartProperties($part);
        $self->reload_tree($part->getPartID);
        # reload_tree implicitly triggeres the _updateWiredRubberbands method in schematic.cpp
        # which is nessecary to have correct wiring!
                
        # trigger slicing steps to update modifications;
		$self->triggerSlicing;

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
    $self->canvas->set_z($z);
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

# trigger slicing steps to update modifications;
sub triggerSlicing {
	my $self = shift;
	
	#$self->{print}->objects->[$self->{obj_idx}]->invalidate_step(STEP_PERIMETERS);
	$self->{print}->objects->[$self->{obj_idx}]->invalidate_step(STEP_SLICE);
	$self->{plater}->start_background_process(1);
}

1;
