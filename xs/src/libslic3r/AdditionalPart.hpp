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


namespace Slic3r {

// Be sure to add new values in AdditionalPart.xsp if changing this enum!
enum PlacingMethod {PM_AUTOMATIC, PM_MANUAL, PM_NONE};
static const std::vector<std::string> PlacingMethodStrings = { "Automatic", "Manual", "None" }; // used for serialization to 3ds file

class AdditionalPart;

class AdditionalPart
{
    public:
    AdditionalPart(std::string name, std::string library, std::string deviceset, std::string device, std::string package);
    ~AdditionalPart();
    unsigned int getPartID() {return this->partID;};
    std::string getName(){return this->name;};
    void printPartInfo();
    double getFootprintHeight(){return this->footprintHeight;};
    void setFootprintHeight(double height){this->footprintHeight = height;};
    void setSize(double x, double y);
    void setSize(double x, double y, double z);
    void setPosition(double x, double y, double z);
    void setPartHeight(double height) {this->size[2] = height;};
    double getPartHeight() {return this->size[2];};
    Pointf3 getPosition() {return this->position;};
    void resetPosition();
    void setRotation(double x, double y, double z);
    Pointf3 getRotation() {return this->rotation;};
    void setPartOrigin(double x, double y, double z);  //internal position of the part, defines origin
    void setVisibility(bool visible) {this->visible = visible;};
    bool isVisible() {return this->visible;};
    void setPlaced(bool placed) {this->placed = placed;};
    bool isPlaced() {return this->placed;};
    void setPlacingMethod(PlacingMethod method) {this->placingMethod = method;};;
    void setPlacingMethod(const std::string method);
    const PlacingMethod getPlacingMethod() {return this->placingMethod;};
    const std::string getPlacingMethodString();
    TriangleMesh getFootprintMesh();
    TriangleMesh getPartMesh();
    TriangleMesh getMesh();
    Polygon getHullPolygon(const double z_lower, const double z_upper, const double hull_offset) const;
    const std::string getPlaceGcode(double print_z, std::string automaticGcode = "", std::string manualGcode = "");
    Point3s getConnectionPoints(const double print_z);
    const std::string getPlaceDescription(Pointf offset);
    void resetPrintedStatus();

    private:
    static unsigned int s_idGenerator;
    unsigned int partID;
    std::string name;
    bool visible;
    bool placed;
    PlacingMethod placingMethod;
    bool printed; // indicates that this part is already included in the GCode

    double size[3];
    Pointf3 position;
    Pointf3 rotation; // is it a good idea to use point as representation for a rotation?
    double origin[3];
    double footprintHeight; // should be set to the layer height of the object to match exactly one layer in the visualization


    // Internal methods to generate a mesh of the object and footprint
    stl_file generateCube(double x, double y, double z, double dx, double dy, double dz);
    stl_file generateCylinder(double x, double y, double z, double r, double h);
    stl_facet generateFacet(double v1x, double v1y, double v1z, double v2x, double v2y, double v2z, double v3x, double v3y, double v3z);
    void merge_stl(stl_file* stl1, stl_file* stl2);
};

}

#endif
