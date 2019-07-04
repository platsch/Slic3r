#include "AdditionalPart.hpp"

namespace Slic3r {

AdditionalPart::AdditionalPart(std::string thread)
{
    this->partID = this->s_idGenerator++;
    this->name = "M" + thread;
    this->thread = thread;

    this->position.x = 0.0;
    this->position.y = 0.0;
    this->position.z = 0.0;
    this->rotation.x = 0.0;
    this->rotation.y = 0.0;
    this->rotation.z = 0.0;

    this->size[0] = 5.0;
    this->size[1] = 5.0;
    this->size[2] = 5.0;

    this->visible = false;
    this->placed = false;
    this->placingMethod = PM_AUTOMATIC;
    this->printed = false;
}

AdditionalPart::~AdditionalPart()
{
}

// Initialization of static ID generator variable
unsigned int AdditionalPart::s_idGenerator = 1;

void AdditionalPart::printPartInfo()
{
    std::cout << "== Part name: " << this->name << " ==" << std::endl;
    std::cout << "ID: " << this->partID << std::endl;
}

// Parts are currently represented only by a simple cube.
// This redefines the size of this cube, which is
// initially obtained from the input schematic file
void AdditionalPart::setSize(double x, double y)
{
    setSize(x, y, this->size[2]);
}

void AdditionalPart::setSize(double x, double y, double z)
{
    this->size[0] = x;
    this->size[1] = y;
    this->size[2] = z;
}

void AdditionalPart::setPosition(double x, double y, double z)
{
    this->position.x = x;
    this->position.y = y;
    this->position.z = z;
}

// reset the part position and rotation to 0. To be used when a part is removed
// from the object but remains part of the schematic.
void AdditionalPart::resetPosition()
{
    this->position.x = 0.0;
    this->position.y = 0.0;
    this->position.z = 0.0;
    this->rotation.x = 0.0;
    this->rotation.y = 0.0;
    this->rotation.z = 0.0;
}

// Rotation angles in deg. Behavior of 2/3 axis rotation still undefined.
void AdditionalPart::setRotation(double x, double y, double z)
{
    this->rotation.x = x;
    this->rotation.y = y;
    this->rotation.z = z;
}

// defines the origin of the part relative to it's body.
void AdditionalPart::setPartOrigin(double x, double y, double z)
{
    this->origin[0] = x;
    this->origin[1] = y;
    this->origin[2] = z;
}

void AdditionalPart::setPlacingMethod(const std::string method)
{
    this->placingMethod = PM_NONE;
    for(int i = 0; i < PlacingMethodStrings.size(); i++) {
        if(method == PlacingMethodStrings[i]) {
            this->placingMethod = (PlacingMethod)i;
        }
    }
}

const std::string AdditionalPart::getPlacingMethodString()
{
    return PlacingMethodStrings[this->placingMethod];
}

TriangleMesh AdditionalPart::getFootprintMesh()
{
    TriangleMesh mesh;
    stl_file stl;
    stl_initialize(&stl);

    mesh.stl = stl;
    mesh.repair();

    mesh.rotate_z(Geometry::deg2rad(this->rotation.z));
    mesh.rotate_y(Geometry::deg2rad(this->rotation.y));
    mesh.rotate_x(Geometry::deg2rad(this->rotation.x));
    mesh.translate(this->position.x, this->position.y, this->position.z);
    return mesh;
}

TriangleMesh AdditionalPart::getPartMesh()
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

TriangleMesh AdditionalPart::getMesh()
{
    TriangleMesh partMesh = this->getPartMesh();
    TriangleMesh footprintMesh = this->getFootprintMesh();
    partMesh.merge(footprintMesh);
    return partMesh;
}

/// Generates the hull polygon of this part between z_lower and z_upper
/// hull_offset is an additional offset to widen the cavity to avoid collisions when inserting a part (unscaled value)
Polygon AdditionalPart::getHullPolygon(const double z_lower, const double z_upper, const double hull_offset) const
{
    Polygon result;
    // part affected?
    if((z_upper-this->position.z > EPSILON)  && z_lower < (this->position.z + this->size[2])) {
        Points points;

        // outline of smd body
        //double dx = this->size[0]/2;
        //double dy = this->size[1]/2;

        int radius = this->size[0];

        for(size_t i = 60; i <= 360; i += 60)
        {
            double rad = i / 180.0 * PI;
            double next_rad = (i - 60) / 180.0 * PI;
            double x = this->origin[0];
            double y = this->origin[1];

            points.push_back(Point(scale_(x + sin(rad) * radius), scale_(y + cos(rad) * radius)));
        }

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
const std::string AdditionalPart::getPlaceGcode(double print_z, std::string automaticGcode, std::string manualGcode)
{
    std::ostringstream gcode;

    if(this->placed && !this->printed && this->position.z + this->size[2] <= print_z) {
        this->printed = true;
        if(this->placingMethod == PM_AUTOMATIC) {
            gcode << ";Automatically place part nr " << this->partID << " " << this->getName() << "\n";
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
            gcode << ";Placing of part nr " << this->partID << " " << this->getName() << " is disabled\n";
        }
    }
    return gcode.str();
}

/* Returns a description of this part in GCode format.
 * print_z parameter as in getPlaceGcode.
 */

// TODO shape
const std::string AdditionalPart::getPlaceDescription(Pointf offset)
{
    std::ostringstream gcode;
    if (this->printed) {
        gcode << ";<part id=\"" << this->partID << "\" name=\"" << this->name << "\">\n";
        gcode << ";  <type identifier=\"hexnut\" thread=\"" << this->thread << "\"/>\n";
        gcode << ";  <position box=\"" << this->partID << "\"/>\n";
        gcode << ";  <size height=\"" << this->size[2] << "\"/>\n";
        gcode << ";  <shape>\n";
        gcode << ";    <point x=\"" <<  (this->origin[0]-this->size[0]/2) << "\" y=\"" << (this->origin[1]-this->size[1]/2) << "\"/>\n";
        gcode << ";    <point x=\"" <<  (this->origin[0]-this->size[0]/2) << "\" y=\"" << (this->origin[1]+this->size[1]/2) << "\"/>\n";
        gcode << ";    <point x=\"" <<  (this->origin[0]+this->size[0]/2) << "\" y=\"" << (this->origin[1]+this->size[1]/2) << "\"/>\n";
        gcode << ";    <point x=\"" <<  (this->origin[0]+this->size[0]/2) << "\" y=\"" << (this->origin[1]-this->size[1]/2) << "\"/>\n";
        gcode << ";  </shape>\n";
        gcode << ";  <destination x=\"" << this->position.x + offset.x << "\" y=\"" << this->position.y + offset.y << "\" z=\"" << this->position.z << "\"/>\n";
        gcode << ";  <rotation x=\"" << this->rotation.x << "\" y=\"" << this->rotation.y << "\" z=\"" << this->rotation.z << "\"/>\n";
        gcode << ";</part>\n\n";
    }
    return gcode.str();
}

void AdditionalPart::resetPrintedStatus()
{
    this->printed = false;
}

stl_file AdditionalPart::generateCube(double x, double y, double z, double dx, double dy, double dz)
{
    stl_file stl;
    stl_initialize(&stl);
    stl_facet facet;


    // x -= dx/2;
    // y -= dy/2;
    int radius = dx;

    for(size_t i = 60; i <= 360; i += 60)
    {
        double rad = i / 180.0 * PI;
        double next_rad = (i - 60) / 180.0 * PI;
        facet = generateFacet(x, y, z, x + sin(rad) * radius, y + cos(rad) * radius, z, x + sin(next_rad) * radius, y + cos(next_rad) * radius, z); //bottom
        stl_add_facet(&stl, &facet);

        facet = generateFacet(
            x + sin(rad) * radius,      y + cos(rad) * radius,      z,
            x + sin(rad) * radius,      y + cos(rad) * radius,      z + dz,
            x + sin(next_rad) * radius, y + cos(next_rad) * radius, z); //bottom
        stl_add_facet(&stl, &facet);
        facet = generateFacet(
            x + sin(rad) * radius,      y + cos(rad) * radius,      z + dz,
            x + sin(next_rad) * radius, y + cos(next_rad) * radius, z + dz,
            x + sin(next_rad) * radius, y + cos(next_rad) * radius, z); //bottom
        stl_add_facet(&stl, &facet);

        facet = generateFacet(x, y, z + dz, x + sin(rad) * radius, y + cos(rad) * radius, z + dz, x + sin(next_rad) * radius, y + cos(next_rad) * radius, z + dz); //bottom
        stl_add_facet(&stl, &facet);
    }

    return stl;
}

stl_file AdditionalPart::generateCylinder(double x, double y, double z, double r, double h)
{
    stl_file stl;
    stl_initialize(&stl);
    stl_facet facet;

    int steps = 16;
    double stepsize = ((360/steps)/180)*PI;
    for(int i = 0; i < steps; i++) {
        facet = generateFacet(x, y, z, x+r*cos((i-1)*stepsize), y+r*sin((i-1)*stepsize), z, x+r*cos(i*stepsize), y+r*sin(i*stepsize), z); //lower part
        stl_add_facet(&stl, &facet);
        facet = generateFacet(x+r*cos((i-1)*stepsize), y+r*sin((i-1)*stepsize), z, x+r*cos(i*stepsize), y+r*sin(i*stepsize), z, x+r*cos(i*stepsize), y+r*sin(i*stepsize), z+h); //outer part
        stl_add_facet(&stl, &facet);
        facet = generateFacet(x+r*cos((i-1)*stepsize), y+r*sin((i-1)*stepsize), z, x+r*cos((i-1)*stepsize), y+r*sin((i-1)*stepsize), z+h, x+r*cos(i*stepsize), y+r*sin(i*stepsize), z+h); //outer part
        stl_add_facet(&stl, &facet);
        facet = generateFacet(x, y, z+h, x+r*cos((i-1)*stepsize), y+r*sin((i-1)*stepsize), z+h, x+r*cos(i*stepsize), y+r*sin(i*stepsize), z+h); //upper part
        stl_add_facet(&stl, &facet);
    }
    return stl;
}

stl_facet AdditionalPart::generateFacet(double v1x, double v1y, double v1z, double v2x, double v2y, double v2z, double v3x, double v3y, double v3z)
{
    stl_facet facet;
    // initialize without normal
    facet.normal.x = 0.0;
    facet.normal.y = 0.0;
    facet.normal.z = 0.0;

    facet.vertex[0].x = v1x;
    facet.vertex[0].y = v1y;
    facet.vertex[0].z = v1z;

    facet.vertex[1].x = v2x;
    facet.vertex[1].y = v2y;
    facet.vertex[1].z = v2z;

    facet.vertex[2].x = v3x;
    facet.vertex[2].y = v3y;
    facet.vertex[2].z = v3z;

    return facet;
}

void AdditionalPart::merge_stl(stl_file* stl, stl_file* other)
{
    for(int i = 0; i < other->stats.number_of_facets; i++) {
        stl_add_facet(stl, &other->facet_start[i]);
    }
}

}
