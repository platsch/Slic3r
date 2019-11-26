#ifndef slic3r_SquareNut_hpp_
#define slic3r_SquareNut_hpp_

#include "libslic3r.h"
#include "Point.hpp"
#include "TriangleMesh.hpp"
#include "Polygon.hpp"
#include "Geometry.hpp"
#include "ClipperUtils.hpp"
#include <vector>
#include <sstream>
#include <algorithm>
#include <admesh/stl.h>
#include "SquareNutSizes.hpp"
#include "FastenerNut.hpp"

namespace Slic3r {

class SquareNut;
typedef std::vector<SquareNut*> SquareNuts;

class SquareNut : public FastenerNut
{
    public:
        SquareNut(std::string threadSize);
        ~SquareNut();
        std::string getType(){return "squarenut";};
        void setZRotation(double z);
        void setPartOrientation(PartOrientation orientation);
        void setPartOrientation(const std::string orientation);
        const PartOrientation getPartOrientation();

        TriangleMesh getPartMesh();
        Polygon getHullPolygon(const double z_lower, const double z_upper, const double hull_offset) const;

        const std::string getPlaceGcode(double print_z, std::string automaticGcode = "", std::string manualGcode = "");
        const std::string getPlaceDescription(Pointf offset);

    private:
        std::string threadSize;
        std::string type;
        PartOrientation partOrientation;

        // Internal methods to generate a mesh of the object and footprint
        stl_file generateSquareNutBody(double x, double y, double z, double diameter, double height);
};

}

#endif
