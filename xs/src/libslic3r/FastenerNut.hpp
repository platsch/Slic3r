#ifndef slic3r_FastenerNut_hpp_
#define slic3r_FastenerNut_hpp_

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
#include "AdditionalPart.hpp"

namespace Slic3r {

enum PartOrientation {PO_FLAT, PO_UPRIGHT};
static const std::vector<std::string> PartOrientationStrings = { "Flat", "Upright" }; // used for serialization to 3ds file

class FastenerNut;
typedef std::vector<FastenerNut*> FastenerNuts;

class FastenerNut : public AdditionalPart
{
    public:
        virtual void setZRotation(double z);
        virtual void setPartOrientation(PartOrientation orientation);
        virtual void setPartOrientation(const std::string orientation);
        virtual const PartOrientation getPartOrientation();

        virtual TriangleMesh getPartMesh();
        virtual Polygon getHullPolygon(const double z_lower, const double z_upper, const double hull_offset);

        virtual const std::string getPlaceGcode(double print_z, std::string automaticGcode = "", std::string manualGcode = "");
        virtual const std::string getPlaceDescription(Pointf offset);

        std::string threadSize;
        PartOrientation partOrientation;

        // Internal methods to generate a mesh of the object and footprint
        virtual stl_file generateSquareNutBody(double x, double y, double z, double diameter, double height);
    };
}

#endif
