#include "SquareNut.hpp"

namespace Slic3r {

    SquareNut::SquareNut(std::string threadSize)
    {
        this->partID = this->s_idGenerator++;
        this->name = "M" + threadSize + " squarenut";
        this->threadSize = threadSize;

        this->position.x = 0.0;
        this->position.y = 0.0;
        this->position.z = 0.0;
        this->rotation.x = 0.0;
        this->rotation.y = 0.0;
        this->rotation.z = 0.0;

        SquareNutSizes::getSize(threadSize, this->size);

        this->visible = false;
        this->placed = false;
        this->placingMethod = PM_AUTOMATIC;
        this->partOrientation = PO_FLAT;
        this->printed = false;
    }

    SquareNut::~SquareNut()
    {
    }

    // Rotation angles in deg.
    void SquareNut::setZRotation(double z)
    {
        this->rotation.z = z;
    }

    const PartOrientation SquareNut::getPartOrientation()
    {
        return this->partOrientation;
    }

    void SquareNut::setPartOrientation(PartOrientation orientation)
    {
        this->partOrientation = orientation;
        if (this->partOrientation == PO_UPRIGHT)
        {
            this->setRotation(90.0, 0.0, this->rotation.z);
        }
        else // flat
        {
            this->setRotation(0, 0, this->rotation.z);
        }
    }

    void SquareNut::setPartOrientation(const std::string orientation)
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

    TriangleMesh SquareNut::getPartMesh()
    {
        TriangleMesh mesh;
        mesh.stl = this->generateSquareNutBody(this->origin[0], this->origin[1], this->origin[2], this->size[0], this->size[2] - this->getFootprintHeight());
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
    Polygon SquareNut::getHullPolygon(const double z_lower, const double z_upper, const double hull_offset) const
    {
        printf("get hull polygon\n");
        printf("origin %d %d %d \n", origin[0], origin[1], origin[2]);
        printf("size %d %d %d \n", size[0], size[1], size[2]);
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

                points.push_back(Point(scale_(x + radius), scale_(z - width)));
                points.push_back(Point(scale_(x - radius), scale_(z - width)));
                points.push_back(Point(scale_(x + radius), scale_(z)));
                points.push_back(Point(scale_(x - radius), scale_(z)));

                result = Slic3r::Geometry::convex_hull(points);

                result = offset(Polygons(result), scale_(hull_offset), 100000, ClipperLib::jtSquare).front();

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
        else
        {
            // part affected?
            if ((z_upper - this->position.z > EPSILON) && z_lower < (this->position.z + this->size[2]))
            {
                Points points;

                double x = this->origin[0];
                double y = this->origin[1];
                double radius = this->size[0] / 2.0;

                points.push_back(Point(scale_(x + radius), scale_(y + radius)));
                points.push_back(Point(scale_(x + radius), scale_(y - radius)));
                points.push_back(Point(scale_(x - radius), scale_(y + radius)));
                points.push_back(Point(scale_(x - radius), scale_(y - radius)));

                // generate polygon
                result = Slic3r::Geometry::convex_hull(points);

                // apply margin to have some space between part an extruded plastic
                result = offset(Polygons(result), scale_(hull_offset), 100000, ClipperLib::jtSquare).front();

                // rotate polygon
                result.rotate(Geometry::deg2rad(this->rotation.z), Point(0, 0));

                // apply object translation
                result.translate(scale_(this->position.x), scale_(this->position.y));
            }
        }

        return result;
    }

    /* Returns the GCode to place this part and remembers this part as already "printed".
    * print_z is the current print layer, a part will only be printed if it's upper surface
    * will be below the current print layer after placing.
    */
    const std::string SquareNut::getPlaceGcode(double print_z, std::string automaticGcode, std::string manualGcode)
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
    const std::string SquareNut::getPlaceDescription(Pointf offset)
    {
        std::ostringstream gcode;
        if (this->printed)
        {
            gcode << ";<part id=\"" << this->partID << "\" name=\"" << this->name << "\">\n";
            gcode << ";  <type identifier=\"squarenut\" thread_size=\"" << this->threadSize << "\"/>\n";
            gcode << ";  <position box=\"" << this->partID << "\"/>\n";
            if (this->partOrientation == PO_UPRIGHT)
            {
                gcode << ";  <size height=\"" << this->size[1] << "\"/>\n";
            }
            else
            {
                gcode << ";  <size height=\"" << this->size[2] << "\"/>\n";
            }
            gcode << ";  <shape>\n";
            gcode << ";    <point x=\"" << (this->origin[0] - this->size[0] / 2.0) << "\" y=\"" << (this->origin[1] - this->size[1] / 2.0) << "\"/>\n";
            gcode << ";    <point x=\"" << (this->origin[0] - this->size[0] / 2.0) << "\" y=\"" << (this->origin[1] + this->size[1] / 2.0) << "\"/>\n";
            gcode << ";    <point x=\"" << (this->origin[0] + this->size[0] / 2.0) << "\" y=\"" << (this->origin[1] + this->size[1] / 2.0) << "\"/>\n";
            gcode << ";    <point x=\"" << (this->origin[0] + this->size[0] / 2.0) << "\" y=\"" << (this->origin[1] - this->size[1] / 2.0) << "\"/>\n";
            gcode << ";  </shape>\n";
            gcode << ";  <destination x=\"" << this->position.x + offset.x << "\" y=\"" << this->position.y + offset.y << "\" z=\"" << this->position.z << "\"/>\n";
            gcode << ";  <orientation orientation=\"" << PartOrientationStrings[this->partOrientation] << "\"/>\n";
            // ignoring anything else than flat and upright rotation right now
            gcode << ";  <rotation z=\"" << this->rotation.z << "\"/>\n";
            gcode << ";</part>\n\n";
        }
        return gcode.str();
    }

    stl_file SquareNut::generateSquareNutBody(double x, double y, double z, double diameter, double height)
    {
        stl_file stl;
        stl_initialize(&stl);
        stl_facet facet;

        double radius = diameter / 2.0;

        // ground plane
        facet = generateFacet(x - radius, y - radius, z,
                            x - radius, y + radius, z,
                            x + radius, y + radius, z);
        stl_add_facet(&stl, &facet);
        facet = generateFacet(x - radius, y - radius, z,
                            x + radius, y + radius, z,
                            x + radius, y - radius, z);
        stl_add_facet(&stl, &facet);
        // cover plane
        facet = generateFacet(x - radius, y - radius, z + height,
                            x - radius, y + radius, z + height,
                            x + radius, y + radius, z + height);
        stl_add_facet(&stl, &facet);
        facet = generateFacet(x - radius, y - radius, z + height,
                            x + radius, y + radius, z + height,
                            x + radius, y - radius, z + height);
        // stl_add_facet(&stl, &facet);
        // side plane left
        facet = generateFacet(x - radius, y - radius, z,
                            x - radius, y + radius, z,
                            x - radius, y + radius, z + height);
        stl_add_facet(&stl, &facet);
        facet = generateFacet(x - radius, y - radius, z + height,
                            x - radius, y + radius, z + height,
                            x - radius, y - radius, z);
        stl_add_facet(&stl, &facet);
        // side plane bottom
        facet = generateFacet(x - radius, y - radius, z,
                            x + radius, y - radius, z,
                            x + radius, y - radius, z + height);
        stl_add_facet(&stl, &facet);
        facet = generateFacet(x - radius, y - radius, z + height,
                            x + radius, y - radius, z + height,
                            x - radius, y - radius, z);
        stl_add_facet(&stl, &facet);
        // side plane right
        facet = generateFacet(x + radius, y - radius, z,
                            x + radius, y + radius, z,
                            x + radius, y + radius, z + height);
        stl_add_facet(&stl, &facet);
        facet = generateFacet(x + radius, y - radius, z + height,
                            x + radius, y + radius, z + height,
                            x + radius, y - radius, z);
        stl_add_facet(&stl, &facet);
        // side plane top
        facet = generateFacet(x - radius, y + radius, z,
                            x + radius, y + radius, z,
                            x + radius, y + radius, z + height);
        stl_add_facet(&stl, &facet);
        facet = generateFacet(x - radius, y + radius, z + height,
                            x + radius, y + radius, z + height,
                            x - radius, y + radius, z);
        stl_add_facet(&stl, &facet);

        return stl;
    }
}
