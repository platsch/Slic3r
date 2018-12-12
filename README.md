## Slic3r Electronics
This is a modified version of Slic3r which supports the integration and routing of electronic components and wires into 3D printable objects.
Your printer should be equipped with an additional extruder for conductive paste to generate wires and (optional) a vacuum gripper / cameras for automatic component placing.
[OctoPNP](https://github.com/platsch/OctoPNP) can be used to control the interactive pick'n place system and for optical calibration.

![screenshot](https://user-images.githubusercontent.com/4190756/35223926-8462bb96-ff83-11e7-9bd3-3b3dbc94f3ef.png)

### Import schematics
The electronics extension currently supports the XML-based Eagle .sch format to import electronic schematics. Importing a schematic will add all components to the list of unplaced parts and generate the connection graph.
Use left-doubleclick on parts, rubberbands and waypoints for placing and routing, right-doubleclick to remove things.

### Export designs
The position of all placed components and waypoints can be ex-/imported to a .3de file. This file must be stored in the same location as the .sch file since it referes to the original schematic for part geometries, connections and unplaced parts.

### Settings
* `print settings`->`Multiple extruders`->`Conductive wire extruder` to select the extruder with conductive material
* `print settings`->`Speed`->`Conductive wires` to set appropriate extrusion speed for conductive material (typically much lower than plastic extrusion speed)
* `print settings`->`3D Electronics` to configure channel widths and offsets

As a starting point, the profile for our developer machine can be found under https://github.com/platsch/Slic3r-Electronics-Config

### Slic3r
Slic3r itself is mainly a **toolpath generator** for 3D printers: it reads 3D models (STL, OBJ, AMF) and it converts them into **G-code** instructions for 3D printers.

for further information on Slic3r, installation and build instructions please refer to the upstream project https://github.com/alexrj/Slic3r or the [Slic3r project homepage](http://slic3r.org/)

### How to install?

If you want to compile the source yourself follow the instructions on one of these wiki pages: 
* [Linux](https://github.com/alexrj/Slic3r/wiki/Running-Slic3r-from-git-on-GNU-Linux)
* [Windows](https://github.com/alexrj/Slic3r/wiki/Running-Slic3r-from-git-on-Windows)
* [Mac OSX](https://github.com/alexrj/Slic3r/wiki/Running-Slic3r-from-git-on-OS-X)

all electronics branches additionally require the libxml2-dev and libxml-perl packages.

### Directory structure

* `Build.PL`: this script installs dependencies into `local-lib/`, compiles the C++ part and runs tests
* `lib/`: the Perl code
* `package/`: the scripts used for packaging the executables
* `slic3r.pl`: the main executable script, launching the GUI and providing the CLI
* `src/`: the C++ source of the `slic3r` executable the and CMake definition file for compiling it (note that this C++ `slic3r` executable can do many things but can't generate G-code yet because the porting isn't finished yet - the main executable is `slic3r.pl`)
* `t/`: the test suite
* `utils/`: various useful scripts
* `xs/src/libslic3r/`: C++ sources for libslic3r
* `xs/src/slic3r/`: C++ sources for the Slic3r GUI application
* `xs/t/`: test suite for libslic3r
* `xs/xsp/`: bindings for calling libslic3r from Perl (XS)

### Acknowledgements

The main author of Slic3r is Alessandro Ranellucci (@alexrj, *Sound* in IRC, [@alranel](http://twitter.com/alranel) on Twitter), who started the project in 2011 and still leads development.

Joseph Lenox (@lordofhyphens, *Loh* in IRC) is the current co-maintainer.
