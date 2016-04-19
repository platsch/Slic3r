#include "ElectronicPart.hpp"

namespace Slic3r {

ElectronicPart::ElectronicPart(std::string name, std::string library, std::string deviceset, std::string device, std::string package)
{
	this->partID = this->s_idGenerator++;
	this->name = name;
	this->library = library;
	this->deviceset = deviceset;
	this->device = device;
	this->package = package;

	this->position.x = 0.0;
	this->position.y = 0.0;
	this->position.z = 0.0;
	this->rotation.x = 0.0;
	this->rotation.y = 0.0;
	this->rotation.z = 0.0;

	// determine part height by package name?
	// default is 1mm. This could be defined by a config value...
	this->size[0] = 0.0;
	this->size[1] = 0.0;
	this->size[2] = 1.0;

	this->visible = false;
	this->placed = false;
}

ElectronicPart::~ElectronicPart()
{
}

// Initialization of static ID generator variable
int ElectronicPart::s_idGenerator = 0;

// Parts are currently represented only by a simple cube.
// This redefines the size of this cube, which is
// initially obtained from the input schematic file
void ElectronicPart::setSize(float x, float y)
{
	setSize(x, y, this->size[2]);
}

void ElectronicPart::setSize(float x, float y, float z)
{
	this->size[0] = x;
	this->size[1] = y;
	this->size[2] = z;
}

void ElectronicPart::setPosition(float x, float y, float z)
{
	this->position.x = x;
	this->position.y = y;
	this->position.z = z;
}

// reset the part position and rotation to 0. To be used when a part is removed
// from the object but remains part of the schematic.
void ElectronicPart::resetPosition()
{
	this->position.x = 0.0;
	this->position.y = 0.0;
	this->position.z = 0.0;
	this->rotation.x = 0.0;
	this->rotation.y = 0.0;
	this->rotation.z = 0.0;
}

// Rotation angles in deg. Behavior of 2/3 axis rotation still undefined.
void ElectronicPart::setRotation(float x, float y, float z)
{
	this->rotation.x = x;
	this->rotation.y = y;
	this->rotation.z = z;
}

// defines the origin of the part relative to it's body.
void ElectronicPart::setPartOrigin(float x, float y, float z)
{
	this->origin[0] = x;
	this->origin[1] = y;
	this->origin[2] = z;
}

void ElectronicPart::addPad(std::string type, std::string pad, std::string pin, std::string gate, float x, float y, float rotation, float dx, float dy, float drill, std::string shape)
{
	ElectronicPad newpad = {
	type,
	pad,
	pin,
	gate,
	{x, y, 0},
	{0, 0, rotation},
	{dx, dy, 0},
	drill,
	shape};

	this->padlist.push_back(newpad);
}

TriangleMesh ElectronicPart::getFootprintMesh()
{
	TriangleMesh mesh;
	stl_file stl;
	stl_initialize(&stl);

	for (std::vector<ElectronicPad>::const_iterator pad = this->padlist.begin(); pad != this->padlist.end(); ++pad) {
		if(pad->type == "smd") {
			stl_file pad_stl = this->generateCube(pad->position[0], pad->position[1], pad->position[2], pad->size[0], pad->size[1], this->getFootprintHeight());
			merge_stl(&stl, &pad_stl);
		}
		if(pad->type == "pad") {
			stl_file pad_stl = this->generateCylinder(pad->position[0], pad->position[1], pad->position[2], pad->drill/2, this->getFootprintHeight());
			merge_stl(&stl, &pad_stl);
		}
	}

	mesh.stl = stl;
	mesh.repair();

	mesh.rotate_z(this->rotation.z);
	mesh.rotate_y(this->rotation.y);
	mesh.rotate_x(this->rotation.x);
	mesh.translate(this->position.x, this->position.y, this->position.z);
	return mesh;
}

TriangleMesh ElectronicPart::getPartMesh()
{
	TriangleMesh mesh;
	mesh.stl = this->generateCube(this->origin[0], this->origin[1], this->origin[2], this->size[0], this->size[1], this->size[2] - this->getFootprintHeight());
	mesh.repair();

	mesh.rotate_z(this->rotation.z);
	mesh.rotate_y(this->rotation.y);
	mesh.rotate_x(this->rotation.x);
	mesh.translate(this->position.x, this->position.y, this->position.z + this->getFootprintHeight());
	return mesh;
}

TriangleMesh ElectronicPart::getMesh()
{
	TriangleMesh partMesh = this->getPartMesh();
	TriangleMesh footprintMesh = this->getFootprintMesh();
	partMesh.merge(footprintMesh);
	return partMesh;
}

stl_file ElectronicPart::generateCube(float x, float y, float z, float dx, float dy, float dz)
{
	stl_file stl;
	stl_initialize(&stl);
	stl_facet facet;


    x -= dx/2;
    y -= dy/2;

    facet = generateFacet(x, y, z, x, y+dy, z, x+dx, y+dy, z); //bottom
    stl_add_facet(&stl, &facet);
    facet = generateFacet(x, y, z, x+dx, y, z, x+dx, y+dy, z); //bottom
    stl_add_facet(&stl, &facet);
    facet = generateFacet(x, y, z, x, y+dy, z, x, y+dy, z+dz); //left
    stl_add_facet(&stl, &facet);
    facet = generateFacet(x, y, z, x, y, z+dz, x, y+dy, z+dz); //left
    stl_add_facet(&stl, &facet);
    facet = generateFacet(x, y+dy, z, x+dx, y+dy, z, x+dx, y+dy, z+dz); //back
    stl_add_facet(&stl, &facet);
    facet = generateFacet(x, y+dy, z, x, y+dy, z+dz, x+dx, y+dy, z+dz); //back
    stl_add_facet(&stl, &facet);
    facet = generateFacet(x+dx, y, z, x+dx, y+dy, z, x+dx, y+dy, z+dz); //right
    stl_add_facet(&stl, &facet);
    facet = generateFacet(x+dx, y, z, x+dx, y, z+dz, x+dx, y+dy, z+dz); //right
    stl_add_facet(&stl, &facet);
    facet = generateFacet(x, y, z, x+dx, y, z, x+dx, y, z+dz); //front
    stl_add_facet(&stl, &facet);
    facet = generateFacet(x, y, z, x, y, z+dz, x+dx, y, z+dz); //front
    stl_add_facet(&stl, &facet);
    facet = generateFacet(x, y, z+dz, x, y+dy, z+dz, x+dx, y+dy, z+dz); //top
    stl_add_facet(&stl, &facet);
    facet = generateFacet(x, y, z+dz, x+dx, y, z+dz, x+dx, y+dy, z+dz); //top
    stl_add_facet(&stl, &facet);

	return stl;
}

stl_file ElectronicPart::generateCylinder(float x, float y, float z, float r, float h)
{
	stl_file stl;
	stl_initialize(&stl);
	stl_facet facet;

	int steps = 16;
	float stepsize = ((360/steps)/180)*3.1415926;
	for(int i = 0; i < steps; i++) {
	    facet = generateFacet(x, y, z, x+r*cos((i-1)*stepsize), y+r*sin((i-1)*stepsize), z, x+r*cos(i*stepsize), y+r*sin(i*stepsize), z); //lower part
	    stl_add_facet(&stl, &facet);
		facet = generateFacet(x+r*cos((i-1)*stepsize), y+r*sin((i-1)*stepsize), z, x+r*cos(i*stepsize), y+r*sin(i*stepsize), z, x+r*cos(i*stepsize), y+r*sin(i*stepsize), z+h); //outer part
		stl_add_facet(&stl, &facet);
		facet = generateFacet(x+r*cos((i-1)*stepsize), y+r*sin((i-1)*stepsize), z, x+r*cos((i-1)*stepsize), y+r*sin((i-1)*stepsize), z+h, x+r*cos(i*stepsize), y+r*sin(i*stepsize), z+h); //outer part
		stl_add_facet(&stl, &facet);
		facet = generateFacet(x, y, z+h, x+r*cos((i-1)*stepsize), y+r*sin((i-1)*stepsize), z+h, x+r*cos(i*stepsize), y+r*sin(i*stepsize), z+h); //upper part
		stl_add_facet(&stl, &facet);
	}
	return stl;
}

stl_facet ElectronicPart::generateFacet(float v1x, float v1y, float v1z, float v2x, float v2y, float v2z, float v3x, float v3y, float v3z)
{
	stl_facet facet;
	// initialize without normal
	facet.normal.x = 0.0;
	facet.normal.y = 0.0;
	facet.normal.z = 0.0;

	facet.vertex[0].x = v1x;
	facet.vertex[0].y = v1y;
	facet.vertex[0].z = v1z;

	facet.vertex[1].x = v2x;
	facet.vertex[1].y = v2y;
	facet.vertex[1].z = v2z;

	facet.vertex[2].x = v3x;
	facet.vertex[2].y = v3y;
	facet.vertex[2].z = v3z;

	return facet;
}

void ElectronicPart::merge_stl(stl_file* stl, stl_file* other)
{
	for(int i = 0; i < other->stats.number_of_facets; i++) {
		stl_add_facet(stl, &other->facet_start[i]);
	}
}

}
