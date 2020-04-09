#ifndef slic3r_ElectronicPart_hpp_
#define slic3r_ElectronicPart_hpp_

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

struct ElectronicPad {
    std::string type;
    std::string pad;
    std::string pin;
    std::string gate;
    double position[3];
    double rotation[3];
    double size[3];
    double drill;
    std::string shape;
};

// Be sure to add new values in ElectronicPart.xsp if changing this enum!
enum PlacingMethod {PM_AUTOMATIC, PM_MANUAL, PM_NONE};
static const std::vector<std::string> PlacingMethodStrings = { "Automatic", "Manual", "None" }; // used for serialization to 3ds file

enum ConnectionMethod {CM_NONE, CM_LAYER, CM_PART};
static const std::vector<std::string> ConnectionMethodStrings = { "None", "Layer", "Part" }; // used for serialization to 3ds file

class ElectronicPart;
typedef std::vector<ElectronicPart*> ElectronicParts;
typedef std::vector<ElectronicPad> Padlist;

class ElectronicPart
{
    public:
    ElectronicPart(std::string name, std::string library, std::string deviceset, std::string device, std::string package);
    ~ElectronicPart();
    unsigned int getPartID() {return this->partID;};
    std::string getName(){return this->name;};
    std::string getLibrary(){return this->library;};
    std::string getDeviceset(){return this->deviceset;};
    std::string getDevice(){return this->device;};
    std::string getPackage(){return this->package;};
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
    void addPad(std::string type, std::string pad, std::string pin, std::string gate, double x, double y, double rotation, double dx, double dy, double drill, std::string shape);
    bool hasPad(std::string padName);
    Pointf3 getAbsPadPosition(std::string padName);
    Pointf3 getAbsPadPositionPerimeter(std::string padName, bool inner);
    void setVisibility(bool visible) {this->visible = visible;};
    bool isVisible() {return this->visible;};
    void setPlaced(bool placed) {this->placed = placed;};
    bool isPlaced() {return this->placed;};
    void setPlacingMethod(PlacingMethod method) {this->placingMethod = method;};;
    void setPlacingMethod(const std::string method);
    const PlacingMethod getPlacingMethod() {return this->placingMethod;};
    const std::string getPlacingMethodString();
    void setConnectionMethod(ConnectionMethod method) {this->connectionMethod = method;};
    void setConnectionMethod(const std::string method);
    const ConnectionMethod getConnectionMethod() {return this->connectionMethod;};
    const std::string getConnectionMethodString();
    Point3s getConnectionPointsLayer(const double print_z);
    Point3s getConnectionPointsPart();
    TriangleMesh getFootprintMesh();
    TriangleMesh getPartMesh();
    TriangleMesh getMesh();
    Polygon getHullPolygon(const double z_lower, const double z_upper, const double hull_offset) const;
    const std::string getPlaceGcode(double print_z, std::string automaticGcode = "", std::string manualGcode = "");
    const std::string getPlaceDescription(Pointf offset);
    void resetPrintedStatus();

    private:
    static unsigned int s_idGenerator;
    unsigned int partID;
    std::string name;
    std::string library;
    std::string deviceset;
    std::string device;
    std::string package;
    bool visible;
    bool placed;
    PlacingMethod placingMethod;
    ConnectionMethod connectionMethod;
    bool printed; // indicates that this part is already included in the GCode
    bool connected; // indicates that this part is already connected by ink drops in the GCode

    double size[3];
    Pointf3 position;
    Pointf3 rotation; // is it a good idea to use point as representation for a rotation?
    double origin[3];
    double footprintHeight; // should be set to the layer height of the object to match exactly one layer in the visualization

    Padlist padlist;

    Point3s _getConnectionPoints();

    // Internal methods to generate a mesh of the object and footprint
    stl_file generateCube(double x, double y, double z, double dx, double dy, double dz);
    stl_file generateCylinder(double x, double y, double z, double r, double h);
    stl_facet generateFacet(double v1x, double v1y, double v1z, double v2x, double v2y, double v2z, double v3x, double v3y, double v3z);
    void merge_stl(stl_file* stl1, stl_file* stl2);

    friend class Schematic;

};

}

#endif
