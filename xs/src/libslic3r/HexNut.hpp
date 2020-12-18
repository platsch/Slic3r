#ifndef slic3r_HexNut_hpp_
#define slic3r_HexNut_hpp_

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
#include "HexNutSizes.hpp"
#include "FastenerNut.hpp"

namespace Slic3r {

class HexNut;
typedef std::vector<HexNut*> HexNuts;

class HexNut : public FastenerNut
{
    public:
        HexNut(std::string threadSize);
        ~HexNut();
        std::string getType(){return "hexnut";};
        void setZRotation(double z);
        void setPartOrientation(PartOrientation orientation);
        void setPartOrientation(const std::string orientation);
        const PartOrientation getPartOrientation();

        TriangleMesh getPartMesh();
        Polygon getHullPolygon(const double z_lower, const double z_upper, const double hull_offset, const bool apply_translation = true) const;

        const std::string getPlaceGcode(double print_z, std::string automaticGcode = "", std::string manualGcode = "");
        const std::string getPlaceDescription(Pointf offset);

    private:
        std::string threadSize;
        std::string type;
        PartOrientation partOrientation;

        // Internal methods to generate a mesh of the object and footprint
        stl_file generateHexNutBody(double x, double y, double z, double diameter, double height);
};

}

#endif
