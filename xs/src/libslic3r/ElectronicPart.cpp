#include "ElectronicPart.hpp"

namespace Slic3r {

ElectronicPart::ElectronicPart(std::string name, std::string library, std::string deviceset, std::string device, std::string package)
{
    this->partID = this->s_idGenerator++;
    this->name = name;
    this->library = library;
    this->deviceset = deviceset;
    this->device = device;
    this->package = package;

    this->position.x = 0.0;
    this->position.y = 0.0;
    this->position.z = 0.0;
    this->rotation.x = 0.0;
    this->rotation.y = 0.0;
    this->rotation.z = 0.0;

    // determine part height by package name?
    // default is 1mm. This could be defined by a config value...
    this->size[0] = 0.0;
    this->size[1] = 0.0;
    this->size[2] = 1.0;

    this->visible = false;
    this->placed = false;
    this->placingMethod = PM_AUTOMATIC;
    this->connectionMethod = CM_NONE;
    this->printed = false;
    this->connected = false;
}

ElectronicPart::~ElectronicPart()
{
}

void ElectronicPart::printPartInfo()
{
    std::cout << "== Part name: " << this->name << " ==" << std::endl;
    std::cout << "ID: " << this->partID << std::endl;
    std::cout << "Library: " << this->library << std::endl;
    std::cout << "Device: " << this->device << std::endl;
    std::cout << "Package: " << this->package << std::endl;
    std::cout << "Pads:" << std::endl;
    for (Padlist::const_iterator pad = this->padlist.begin(); pad != this->padlist.end(); ++pad) {
        std::cout << "   -type: " << pad->type << std::endl;
        std::cout << "   -pad: " << pad->pad << std::endl;
        std::cout << "   -ping: " << pad->pin << std::endl;
        std::cout << "   -gate: " << pad->gate << std::endl;
        }
    }

    void ElectronicPart::addPad(std::string type, std::string pad, std::string pin, std::string gate, double x, double y, double rotation, double dx, double dy, double drill, std::string shape)
    {
        ElectronicPad newpad = {
        type,
        pad,
        pin,
        gate,
        {x, y, 0},
        {0, 0, rotation},
        {dx, dy, 0},
        drill,
        shape};

        this->padlist.push_back(newpad);
    }

    /* Check whether this part has a pad whith the name given.
    * Intended to be used before calling getAbsPadPosition to ensure a valid result.
    */
    bool ElectronicPart::hasPad(std::string padName)
    {
        bool result = false;
        for (Padlist::const_iterator pad = this->padlist.begin(); pad != this->padlist.end(); ++pad) {
            if((pad->pad == padName) || pad->pin == padName) {
                result = true;
            }
        }
        return result;
    }

    Pointf3 ElectronicPart::getAbsPadPosition(std::string padName)
    {
        Pointf3 pos;
        for (Padlist::const_iterator pad = this->padlist.begin(); pad != this->padlist.end(); ++pad) {
            if((pad->pad == padName) || (pad->pin == padName)) {
                pos.x = pad->position[0];
                pos.y = pad->position[1];
                pos.z = pad->position[2];

                pos.rotate(Geometry::deg2rad(this->rotation.z), Pointf(0, 0));
                // rotation around y and z is missing!!!
                pos.translate(this->position);
                // set pad center
                pos.translate(0, 0, this->footprintHeight/2);
            }
        }
        return pos;
    }

    /* Find the point on the outline of this pad closest to the center of the part
    * to extend the conductive extrusion to this end of the pad. Can find the inner or outer
    * point for cases where the trace is connected from below the part.
    */
    Pointf3 ElectronicPart::getAbsPadPositionPerimeter(std::string padName, bool inner)
    {
        Pointf3 pos[4];
        Pointf3 orig(this->origin[0], this->origin[1], this->origin[2]);
        Pointf3 result(0, 0, 0);
            for (Padlist::const_iterator pad = this->padlist.begin(); pad != this->padlist.end(); ++pad) {
                if((pad->pad == padName) || (pad->pin == padName)) {

                    // find side with minimum distance to the part's origin
                    double dx = pad->size[0]/2;
                    double dy = pad->size[1]/2;

                    pos[0].x = pad->position[0]+dx - this->origin[0];
                    pos[0].y = pad->position[1] - this->origin[1];
                    pos[0].z = this->origin[2];

                    pos[1].x = pad->position[0]-dx - this->origin[0];
                    pos[1].y = pad->position[1] - this->origin[1];
                    pos[1].z = this->origin[2];

                    pos[2].x = pad->position[0] - this->origin[0];
                    pos[2].y = pad->position[1]+dy - this->origin[1];
                    pos[2].z = this->origin[2];

                    pos[3].x = pad->position[0] - this->origin[0];
                    pos[3].y = pad->position[1]-dy - this->origin[1];
                    pos[3].z = this->origin[2];

                    result = pos[0];

                    // are we looking for the inner our outer point?
                    for(int i = 1; i < 4; i++) {
                        if(inner) {
                            if(orig.distance_to(pos[i]) < orig.distance_to(result)) {
                                result = pos[i];
                            }
                        }else{
                            if(orig.distance_to(pos[i]) > orig.distance_to(result)) {
                                result = pos[i];
                            }
                        }
                    }

                    result.rotate(Geometry::deg2rad(this->rotation.z), Pointf(0, 0));
                    // rotation around y and z is missing!!!
                    result.translate(this->position);
                    // set pad center
                    result.translate(0, 0, this->footprintHeight/2);

                    break;
                }
            }
            return result;
    }

    void ElectronicPart::setConnectionMethod(const std::string method)
    {
        this->connectionMethod = CM_NONE;
        for(int i = 0; i < ConnectionMethodStrings.size(); i++) {
            if(method == ConnectionMethodStrings[i]) {
                this->connectionMethod = (ConnectionMethod)i;
            }
        }
    }
    const std::string ElectronicPart::getConnectionMethodString()
    {
        return ConnectionMethodStrings[this->connectionMethod];
    }

    TriangleMesh ElectronicPart::getFootprintMesh()
    {
        TriangleMesh mesh;
        stl_file stl;
        stl_initialize(&stl);

        for (Padlist::const_iterator pad = this->padlist.begin(); pad != this->padlist.end(); ++pad) {
            if(pad->type == "smd") {
                stl_file pad_stl = this->generateCube(pad->position[0], pad->position[1], pad->position[2], pad->size[0], pad->size[1], this->getFootprintHeight());
                merge_stl(&stl, &pad_stl);
            }
            if(pad->type == "pad") {
                stl_file pad_stl = this->generateCylinder(pad->position[0], pad->position[1], pad->position[2], pad->drill/2, this->getFootprintHeight());
                merge_stl(&stl, &pad_stl);
            }
        }

        mesh.stl = stl;
        mesh.repair();

        mesh.rotate_z(Geometry::deg2rad(this->rotation.z));
        mesh.rotate_y(Geometry::deg2rad(this->rotation.y));
        mesh.rotate_x(Geometry::deg2rad(this->rotation.x));
        mesh.translate(this->position.x, this->position.y, this->position.z);
        return mesh;
    }

    TriangleMesh ElectronicPart::getPartMesh()
    {
        TriangleMesh mesh;
        mesh.stl = this->generateCube(this->origin[0], this->origin[1], this->origin[2], this->size[0], this->size[1], this->size[2] - this->getFootprintHeight());
        mesh.repair();

        mesh.rotate_z(Geometry::deg2rad(this->rotation.z));
        mesh.rotate_y(Geometry::deg2rad(this->rotation.y));
        mesh.rotate_x(Geometry::deg2rad(this->rotation.x));
        mesh.translate(this->position.x, this->position.y, this->position.z + this->getFootprintHeight());
        return mesh;
    }

    TriangleMesh ElectronicPart::getMesh()
    {
        TriangleMesh partMesh = this->getPartMesh();
        TriangleMesh footprintMesh = this->getFootprintMesh();
        partMesh.merge(footprintMesh);
        return partMesh;
    }

    /// Generates the hull polygon of this part between z_lower and z_upper
    /// hull_offset is an additional offset to widen the cavity to avoid collisions when inserting a part (unscaled value)
    Polygon ElectronicPart::getHullPolygon(const double z_lower, const double z_upper, const double hull_offset) const
    {
        Polygon result;
        // part affected?
        if((z_upper - this->position.z > EPSILON)  && z_lower < (this->position.z + this->size[2])) {
            Points points;
            // outline of smd (and TH) pads
            for (Padlist::const_iterator pad = this->padlist.begin(); pad != this->padlist.end(); ++pad) {
                if(pad->type == "smd") {
                    double dx = pad->size[0]/2;
                    double dy = pad->size[1]/2;
                    points.push_back(Point(scale_(pad->position[0]+dx), scale_(pad->position[1]+dy)));
                    points.push_back(Point(scale_(pad->position[0]+dx), scale_(pad->position[1]-dy)));
                    points.push_back(Point(scale_(pad->position[0]-dx), scale_(pad->position[1]-dy)));
                    points.push_back(Point(scale_(pad->position[0]-dx), scale_(pad->position[1]+dy)));
                }
                if(pad->type == "pad") {
                    // cylinder...
                }
            }

            // outline of smd body
            double dx = this->size[0]/2;
            double dy = this->size[1]/2;
            points.push_back(Point(scale_(this->origin[0]+dx), scale_(this->origin[1]+dy)));
            points.push_back(Point(scale_(this->origin[0]+dx), scale_(this->origin[1]-dy)));
            points.push_back(Point(scale_(this->origin[0]-dx), scale_(this->origin[1]-dy)));
            points.push_back(Point(scale_(this->origin[0]-dx), scale_(this->origin[1]+dy)));

            // generate polygon
            result = Slic3r::Geometry::convex_hull(points);

            // apply margin to have some space between part an extruded plastic
            result = offset(Polygons(result), scale_(hull_offset), 100000, ClipperLib::jtSquare).front();

            // rotate polygon
            result.rotate(Geometry::deg2rad(this->rotation.z), Point(0,0));

            // apply object translation
            result.translate(scale_(this->position.x), scale_(this->position.y));
        }

        return result;
    }


    /* Returns the GCode to place this part and remembers this part as already "printed".
    * print_z is the current print layer, a part will only be printed if it's upper surface
    * will be below the current print layer after placing.
    */
    const std::string ElectronicPart::getPlaceGcode(double print_z, std::string automaticGcode, std::string manualGcode)
    {
        std::ostringstream gcode;

        if(this->placed && !this->printed && this->position.z + this->size[2] <= print_z) {
            this->printed = true;
            if(this->placingMethod == PM_AUTOMATIC) {
                gcode << ";Automatically place part nr " << this->partID << "\n";
                gcode << "M361 P" << this->partID << "\n";
                gcode << automaticGcode << "\n";
            }
            if(this->placingMethod == PM_MANUAL) {
                gcode << ";Manually place part nr " << this->partID << "\n";
                gcode << "M117 Insert component " << this->partID << " " << this->name << "\n";
                //gcode << "M363 P" << this->partID << "\n";
                gcode << manualGcode << "\n";
                // TODO: implement placeholder variable replacement
            }
            if(this->placingMethod == PM_NONE) {
                gcode << ";Placing of part nr " << this->partID << " is disabled\n";
            }
        }
        return gcode.str();
    }

    /* Returns a set of points with coordinates of all pads in scaled coordinates
    * if the connection method is not set to none and if the print_z is
    * higher than the base or upper surface (depending on the connection method).
    * Requires resetPrintedStatus() before being executed again.
    */
    Point3s ElectronicPart::getConnectionPoints(const double print_z)
    {
        Point3s result;
        // part affected?
        if(this->placed && !this->connected) {
            // matching height?
            if((this->connectionMethod == CM_LAYER && print_z - this->position.z > EPSILON)
                || (this->connectionMethod == CM_PART && this->position.z + this->size[2] <= print_z)) {

                for (Padlist::const_iterator pad = this->padlist.begin(); pad != this->padlist.end(); ++pad) {
                    result.push_back(Point3(scale_(pad->position[0]), scale_(pad->position[1]), scale_(pad->position[2])));
                }

                for (Point3s::iterator it = result.begin(); it != result.end(); ++it) {
                    // rotate points
                    it->rotate_z(Geometry::deg2rad(this->rotation.z));

                    // apply object translation
                    it->translate(scale_(this->position.x), scale_(this->position.y), scale_(this->position.z));
                }

                // remember status
                this->connected = true;
            }
        }

        return result;
    }

    /* Returns a description of this part in GCode format.
    * print_z parameter as in getPlaceGcode.
    */
    const std::string ElectronicPart::getPlaceDescription(Pointf offset)
    {
        std::ostringstream gcode;
        if (this->printed) {
            gcode << ";<part id=\"" << this->partID << "\" name=\"" << this->name << "\">\n";
            gcode << ";  <type identifier=\"electronic\"/>\n";
            gcode << ";  <position box=\"" << this->partID << "\"/>\n";
            gcode << ";  <size height=\"" << this->size[2] << "\"/>\n";
            gcode << ";  <shape>\n";
            gcode << ";    <point x=\"" <<  (this->origin[0]-this->size[0]/2) << "\" y=\"" << (this->origin[1]-this->size[1]/2) << "\"/>\n";
            gcode << ";    <point x=\"" <<  (this->origin[0]-this->size[0]/2) << "\" y=\"" << (this->origin[1]+this->size[1]/2) << "\"/>\n";
            gcode << ";    <point x=\"" <<  (this->origin[0]+this->size[0]/2) << "\" y=\"" << (this->origin[1]+this->size[1]/2) << "\"/>\n";
            gcode << ";    <point x=\"" <<  (this->origin[0]+this->size[0]/2) << "\" y=\"" << (this->origin[1]-this->size[1]/2) << "\"/>\n";
            gcode << ";  </shape>\n";
            gcode << ";  <pads>\n";
            for (Padlist::const_iterator pad = this->padlist.begin(); pad != this->padlist.end(); ++pad) {
                gcode << ";    <pad x1=\"" << (pad->position[0]-pad->size[0]/2) << \
                        "\" y1=\"" << (pad->position[1]-pad->size[1]/2) << \
                        "\" x2=\"" << (pad->position[0]+pad->size[0]/2) << \
                        "\" y2=\"" << (pad->position[1]+pad->size[1]/2) << "\"/>\n";
            }
            gcode << ";  </pads>\n";
            gcode << ";  <destination x=\"" << this->position.x + offset.x << "\" y=\"" << this->position.y + offset.y << "\" z=\"" << this->position.z << "\" orientation=\"" << this->rotation.z << "\"/>\n";
            gcode << ";</part>\n\n";
        }
        return gcode.str();
    }
}
