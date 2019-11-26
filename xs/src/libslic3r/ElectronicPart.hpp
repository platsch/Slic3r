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
#include "AdditionalPart.hpp"


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
enum ConnectionMethod {CM_NONE, CM_LAYER, CM_PART};
static const std::vector<std::string> ConnectionMethodStrings = { "None", "Layer", "Part" }; // used for serialization to 3ds file

class ElectronicPart;
typedef std::vector<ElectronicPart*> ElectronicParts;
typedef std::vector<ElectronicPad> Padlist;

class ElectronicPart : public AdditionalPart
{
    public:
    ElectronicPart(std::string name, std::string library, std::string deviceset, std::string device, std::string package);
    ~ElectronicPart();
    std::string getLibrary(){return this->library;};
    std::string getDeviceset(){return this->deviceset;};
    std::string getDevice(){return this->device;};
    std::string getPackage(){return this->package;};
    void printPartInfo();

    void addPad(std::string type, std::string pad, std::string pin, std::string gate, double x, double y, double rotation, double dx, double dy, double drill, std::string shape);
    bool hasPad(std::string padName);
    Pointf3 getAbsPadPosition(std::string padName);
    Pointf3 getAbsPadPositionPerimeter(std::string padName, bool inner);
    void setConnectionMethod(ConnectionMethod method) {this->connectionMethod = method;};
    void setConnectionMethod(const std::string method);
    const ConnectionMethod getConnectionMethod() {return this->connectionMethod;};
    const std::string getConnectionMethodString();
    TriangleMesh getFootprintMesh();
    TriangleMesh getPartMesh();
    TriangleMesh getMesh();
    Polygon getHullPolygon(const double z_lower, const double z_upper, const double hull_offset) const;
    const std::string getPlaceGcode(double print_z, std::string automaticGcode = "", std::string manualGcode = "");
    Point3s getConnectionPoints(const double print_z);
    const std::string getPlaceDescription(Pointf offset);

    private:
    std::string library;
    std::string deviceset;
    std::string device;
    std::string package;
    ConnectionMethod connectionMethod;
    bool connected; // indicates that this part is already connected by ink drops in the GCode

    Padlist padlist;

    friend class Schematic;

};

}

#endif
