#ifndef slic3r_ElectronicPart_hpp_
#define slic3r_ElectronicPart_hpp_

#include "libslic3r.h"
#include "Point.hpp"
#include "TriangleMesh.hpp"
#include <vector>
#include <admesh/stl.h>


namespace Slic3r {

struct ElectronicPad {
	std::string type;
	std::string pad;
	std::string pin;
	std::string gate;
	float position[3];
	float rotation[3];
	float size[3];
	float drill;
	std::string shape;
};

class ElectronicPart;
typedef std::vector<ElectronicPart*> ElectronicParts;

class ElectronicPart
{
    public:
    ElectronicPart(std::string name, std::string library, std::string deviceset, std::string device, std::string package);
    ~ElectronicPart();
    int getPartID() {return this->partID;};
    std::string getName(){return this->name;};
    std::string getLibrary(){return this->library;};
    std::string getDeviceset(){return this->deviceset;};
    std::string getDevice(){return this->device;};
    std::string getPackage(){return this->package;};
    float getFootprintHeight(){return this->footprintHeight;};
    void setFootprintHeight(float height){this->footprintHeight = height;};
    void setSize(float x, float y);
    void setSize(float x, float y, float z);
    void setPosition(float x, float y, float z);
    void setPartHeight(float height) {this->size[2] = height;};
    float getPartHeight() {return this->size[2];};
    Pointf3 getPosition() {return this->position;};
    void resetPosition();
    void setRotation(float x, float y, float z);
    Pointf3 getRotation() {return this->rotation;};
    void setPartOrigin(float x, float y, float z);  //internal position of the part, defines origin
    void addPad(std::string type, std::string pad, std::string pin, std::string gate, float x, float y, float rotation, float dx, float dy, float drill, std::string shape);
    void setVisibility(bool visible) {this->visible = visible;};
    bool isVisible() {return this->visible;};
    void setPlaced(bool placed) {this->placed = placed;};
    bool isPlaced() {return this->placed;};
    TriangleMesh getFootprintMesh();
    TriangleMesh getPartMesh();
    TriangleMesh getMesh();

	private:
    static int s_idGenerator;
    int partID;
	std::string name;
	std::string library;
	std::string deviceset;
	std::string device;
	std::string package;
	bool visible;
	bool placed;

	float size[3];
	Pointf3 position;
	Pointf3 rotation; // is it a good idea to use point as representation for a rotation?
	float origin[3];
	float footprintHeight; // should be set to the layer height of the object to match exactly one layer in the visualization

	std::vector<ElectronicPad> padlist;

	// Internal methods to generate a mesh of the object and footprint
	stl_file generateCube(float x, float y, float z, float dx, float dy, float dz);
	stl_file generateCylinder(float x, float y, float z, float r, float h);
	stl_facet generateFacet(float v1x, float v1y, float v1z, float v2x, float v2y, float v2z, float v3x, float v3y, float v3z);
	void merge_stl(stl_file* stl1, stl_file* stl2);


};

}

#endif
