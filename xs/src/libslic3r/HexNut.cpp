#include "HexNut.hpp"

namespace Slic3r {

HexNut::HexNut(std::string threadSize)
{
    this->partID = this->s_idGenerator++;
    this->name = "M" + threadSize + " hexnut";
    this->threadSize = threadSize;

    this->position.x = 0.0;
    this->position.y = 0.0;
    this->position.z = 0.0;
    this->rotation.x = 0.0;
    this->rotation.y = 0.0;
    this->rotation.z = 0.0;
    this->origin[0] = 0.0;
    this->origin[1] = 0.0;
    this->origin[2] = 0.0;

    HexNutSizes::getSize(threadSize, this->size);

    this->visible = false;
    this->placed = false;
    this->placingMethod = PM_AUTOMATIC;
    this->partOrientation = PO_FLAT;
    this->printed = false;
}

HexNut::~HexNut()
{
}

// Rotation angles in deg.
void HexNut::setZRotation(double z)
{
    this->rotation.z = z;
}

const PartOrientation HexNut::getPartOrientation()
{
    return this->partOrientation;
}

void HexNut::setPartOrientation(PartOrientation orientation)
{
    this->partOrientation = orientation;
    if (this->partOrientation == PO_UPRIGHT)
    {
        this->setRotation(90.0, 30.0, this->rotation.z);
    }
    else // flat
    {
        this->setRotation(0, 0, this->rotation.z);
    }
}

void HexNut::setPartOrientation(const std::string orientation)
{
    this->partOrientation = PO_FLAT;
    for (int i = 0; i < PartOrientationStrings.size(); i++)
    {
        if (orientation == PartOrientationStrings[i])
        {
            this->setPartOrientation((PartOrientation)i);
        }
    }
}

TriangleMesh HexNut::getPartMesh()
{
    TriangleMesh mesh;
    mesh.stl = this->generateHexNutBody(this->origin[0], this->origin[1], this->origin[2], this->size[0], this->size[2] - this->getFootprintHeight());
    mesh.repair();

    mesh.rotate_x(Geometry::deg2rad(this->rotation.x));
    mesh.rotate_y(Geometry::deg2rad(this->rotation.y));
    mesh.rotate_z(Geometry::deg2rad(this->rotation.z));
    if(this->partOrientation == PO_FLAT)
    {
        mesh.translate(this->position.x, this->position.y, this->position.z + this->getFootprintHeight());
    }
    else
    {
        double thickness = this->size[2];
        mesh.translate(this->position.x + thickness / 2.0 * -sin(Geometry::deg2rad(this->rotation.z)), this->position.y + thickness / 2.0 * cos(Geometry::deg2rad(this->rotation.z)), this->position.z + this->size[1] / 2.0);
    }

    return mesh;
}

/// Generates the hull polygon of this part between z_lower and z_upper
/// hull_offset is an additional offset to widen the cavity to avoid collisions when inserting a part (unscaled value)
Polygon HexNut::getHullPolygon(const double z_lower, const double z_upper, const double hull_offset, const bool apply_translation) const
{
    Polygon result;
    // check if object is upright
    if (this->partOrientation == PO_UPRIGHT)
    {
        // part affected?
        if (z_lower >= this->position.z && z_upper < this->position.z + this->size[1] + EPSILON)
        {
            Points points;

            double radius = this->size[0] / 2.0;

            double x = this->origin[0];
            double z = this->origin[1];
            double width = this->size[2];

            // lower half affected?
            if (z_lower < this->position.z + this->size[1] / 2.0)
            {
                double height = this->position.z - z_lower;
                double crossSectionLength = radius - tan(Geometry::deg2rad(30)) * height * 2;

                points.push_back(Point(scale_(x + crossSectionLength / 2.0), scale_(z - width)));
                points.push_back(Point(scale_(x + crossSectionLength / 2.0), scale_(z)));
                points.push_back(Point(scale_(x - crossSectionLength / 2.0), scale_(z - width)));
                points.push_back(Point(scale_(x - crossSectionLength / 2.0), scale_(z)));
            }
            // upper half building straight up
            else
            {
                points.push_back(Point(scale_(x + radius), scale_(z - width)));
                points.push_back(Point(scale_(x - radius), scale_(z - width)));
                points.push_back(Point(scale_(x + radius), scale_(z)));
                points.push_back(Point(scale_(x - radius), scale_(z)));
            }

            result = Slic3r::Geometry::convex_hull(points);

            result = offset(Polygons(result), scale_(hull_offset), 100000, ClipperLib::jtSquare).front();

            if(apply_translation) {
                // rotate polygon
                result.rotate(Geometry::deg2rad(this->rotation.z), Point(0, 0));

                // apply object translation
                if (this->partOrientation == PO_UPRIGHT)
                {
                    double thickness = this->size[2];
                    result.translate(scale_(this->position.x + thickness / 2.0 * -sin(Geometry::deg2rad(this->rotation.z))), scale_(this->position.y + thickness / 2.0 * cos(Geometry::deg2rad(this->rotation.z))));
                }
                else
                {
                    result.translate(scale_(this->position.x), scale_(this->position.y));

                }
            }
        }
    }
    else
    {
        // part affected?
        if ((z_upper - this->position.z > EPSILON) && z_lower < (this->position.z + this->size[2]))
        {
            Points points;

            double x = this->origin[0];
            double y = this->origin[1];
            double radius = this->size[0] / 2.0;

            for (size_t i = 60; i <= 360; i += 60)
            {
                double rad = i / 180.0 * PI;
                double next_rad = (i - 60) / 180.0 * PI;

                points.push_back(Point(scale_(x + sin(rad) * radius), scale_(y + cos(rad) * radius)));
            }

            // generate polygon
            result = Slic3r::Geometry::convex_hull(points);

            // apply margin to have some space between part an extruded plastic
            result = offset(Polygons(result), scale_(hull_offset), 100000, ClipperLib::jtSquare).front();

            if(apply_translation) {
                // rotate polygon
                result.rotate(Geometry::deg2rad(this->rotation.z), Point(0,0));

                // apply object translation
                result.translate(scale_(this->position.x), scale_(this->position.y));
            }
        }
    }

    return result;
}

/* Returns the GCode to place this part and remembers this part as already "printed".
 * print_z is the current print layer, a part will only be printed if it's upper surface
 * will be below the current print layer after placing.
 */
const std::string HexNut::getPlaceGcode(double print_z, std::string automaticGcode, std::string manualGcode)
{
    std::ostringstream gcode;

    if (this->placed && !this->printed && this->position.z + this->size[2] <= print_z)
    {
        this->printed = true;
        if (this->placingMethod == PM_AUTOMATIC)
        {
            gcode << ";Automatically place part nr " << this->partID << " " << this->getName() << "\n";
            gcode << "M361 P" << this->partID << "\n";
            gcode << automaticGcode << "\n";
        }
        if (this->placingMethod == PM_MANUAL)
        {
            gcode << ";Manually place part nr " << this->partID << "\n";
            gcode << "M117 Insert component " << this->partID << " " << this->name << "\n";
            //gcode << "M363 P" << this->partID << "\n";
            gcode << manualGcode << "\n";
            // TODO: implement placeholder variable replacement
        }
        if (this->placingMethod == PM_NONE)
        {
            gcode << ";Placing of part nr " << this->partID << " " << this->getName() << " is disabled\n";
        }
    }
    return gcode.str();
}

/* Returns a description of this part in GCode format.
 * print_z parameter as in getPlaceGcode.
 */

const std::string HexNut::getPlaceDescription(Pointf offset)
{
    std::ostringstream gcode;
    if (this->printed)
    {
        gcode << ";<part id=\"" << this->partID << "\" name=\"" << this->name << "\">\n";
        gcode << ";  <type identifier=\"hexnut\" thread_size=\"" << this->threadSize << "\"/>\n";
        gcode << ";  <position box=\"" << this->partID << "\"/>\n";
        double z_pos = this->position.z;
        if (this->partOrientation == PO_UPRIGHT)
        {
            gcode << ";  <size height=\"" << this->size[1] << "\"/>\n";
            z_pos += this->size[1] / 2.0;
        }
        if (this->partOrientation == PO_FLAT)
        {
            gcode << ";  <size height=\"" << this->size[2] << "\"/>\n";
            z_pos += this->size[2] / 2.0;
        }
        std::cout << "z_pos: " << z_pos << std::endl;
        Polygon shape = this->getHullPolygon(z_pos, z_pos+EPSILON, 0, false);
        gcode << ";  <shape>\n";
        for(Points::const_iterator p = shape.points.begin(); p != shape.points.end(); ++p) {
         gcode << ";    <point x=\"" << unscale(p->x) << "\" y=\"" << unscale(p->y) << "\"/>\n";
        }
        //gcode << ";    <point x=\"" << (this->origin[0] - this->size[0] / 2.0) << "\" y=\"" << (this->origin[1] - this->size[1] / 2.0) << "\"/>\n";
        //gcode << ";    <point x=\"" << (this->origin[0] - this->size[0] / 2.0) << "\" y=\"" << (this->origin[1] + this->size[1] / 2.0) << "\"/>\n";
        //gcode << ";    <point x=\"" << (this->origin[0] + this->size[0] / 2.0) << "\" y=\"" << (this->origin[1] + this->size[1] / 2.0) << "\"/>\n";
        //gcode << ";    <point x=\"" << (this->origin[0] + this->size[0] / 2.0) << "\" y=\"" << (this->origin[1] - this->size[1] / 2.0) << "\"/>\n";
        gcode << ";  </shape>\n";
        gcode << ";  <destination x=\"" << this->position.x + offset.x << "\" y=\"" << this->position.y + offset.y << "\" z=\"" << this->position.z << "\"/>\n";
        gcode << ";  <orientation orientation=\"" << PartOrientationStrings[this->partOrientation] << "\"/>\n";
        // ignoring anything else than flat and upright rotation right now
        gcode << ";  <rotation z=\"" << this->rotation.z << "\"/>\n";
        gcode << ";</part>\n\n";
    }
    return gcode.str();
}

stl_file HexNut::generateHexNutBody(double x, double y, double z, double diameter, double height)
{
    stl_file stl;
    stl_initialize(&stl);
    stl_facet facet;

    double radius = diameter / 2.0;

    for(size_t i = 60; i <= 360; i += 60)
    {
        double rad = i / 180.0 * PI;
        double next_rad = (i - 60) / 180.0 * PI;
        facet = generateFacet(x, y, z, x + sin(rad) * radius, y + cos(rad) * radius, z, x + sin(next_rad) * radius, y + cos(next_rad) * radius, z); //bottom
        stl_add_facet(&stl, &facet);

        facet = generateFacet(
            x + sin(rad) * radius,      y + cos(rad) * radius,      z,
            x + sin(rad) * radius,      y + cos(rad) * radius,      z + height,
            x + sin(next_rad) * radius, y + cos(next_rad) * radius, z); //bottom
        stl_add_facet(&stl, &facet);
        facet = generateFacet(
            x + sin(rad) * radius,      y + cos(rad) * radius,      z + height,
            x + sin(next_rad) * radius, y + cos(next_rad) * radius, z + height,
            x + sin(next_rad) * radius, y + cos(next_rad) * radius, z); //bottom
        stl_add_facet(&stl, &facet);

        facet = generateFacet(x, y, z + height, x + sin(rad) * radius, y + cos(rad) * radius, z + height, x + sin(next_rad) * radius, y + cos(next_rad) * radius, z + height); //bottom
        stl_add_facet(&stl, &facet);
    }

    return stl;
}
}
