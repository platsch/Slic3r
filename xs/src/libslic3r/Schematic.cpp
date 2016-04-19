#include "Schematic.hpp"

namespace Slic3r {


Schematic::Schematic()
{
	this->rootOffset = new Pointf3(0, 0, 0);
	this->filename = "Default file";
}

Schematic::~Schematic()
{
}

void Schematic::addElectronicPart(ElectronicPart* part)
{
	this->partlist.push_back(part);
}

ElectronicPart* Schematic::addElectronicPart(std::string name, std::string library, std::string deviceset, std::string device, std::string package)
{
	addElectronicPart(new ElectronicPart(name, library, deviceset, device, package));
	return this->partlist.back();
}

void Schematic::addElectronicNet(ElectronicNet* net)
{
	this->netlist.push_back(net);
}

ElectronicParts* Schematic::getPartlist()
{
	return &this->partlist;
}

void Schematic::setRootOffset(Pointf3 offset)
{
	delete this->rootOffset;
	this->rootOffset = new Pointf3(offset.x, offset.y, offset.z);
}

Pointf3 Schematic::getRootOffset()
{
	return *(this->rootOffset);
}

void Schematic::setFilename(std::string filename)
{
	this->filename = filename;
}

}
