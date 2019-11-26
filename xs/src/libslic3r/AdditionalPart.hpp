#ifndef slic3r_AdditionalPart_hpp_
#define slic3r_AdditionalPart_hpp_

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
#include "SquareNutSizes.hpp"
#include "AP.hpp"

namespace Slic3r {

enum PartOrientation {PO_FLAT, PO_UPRIGHT};
static const std::vector<std::string> PartOrientationStrings = { "Flat", "Upright" }; // used for serialization to 3ds file

class AdditionalPart;
typedef std::vector<AdditionalPart*> AdditionalParts;

class AdditionalPart : public AP
{
    public:
    AdditionalPart(std::string name, std::string type);
    ~AdditionalPart();
    std::string getType(){return this->type;};
    void setZRotation(double z);
    void setPartOrientation(PartOrientation orientation);
    void setPartOrientation(const std::string orientation);
    const PartOrientation getPartOrientation();

    TriangleMesh getPartMesh();
    Polygon getHullPolygon(const double z_lower, const double z_upper, const double hull_offset);

    const std::string getPlaceGcode(double print_z, std::string automaticGcode = "", std::string manualGcode = "");
    const std::string getPlaceDescription(Pointf offset);

private:
    std::string threadSize;
    std::string type;
    PartOrientation partOrientation;

    // Internal methods to generate a mesh of the object and footprint
    stl_file generateHexNutBody(double x, double y, double z, double diameter, double height);
    stl_file generateSquareNutBody(double x, double y, double z, double diameter, double height);
};

}

#endif
