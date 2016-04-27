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

RubberBandPtrs* Schematic::getRubberBands()
{

	// clear old vector of rubber-bands
	this->rubberBands.clear();

	// update rubberband information
	for (ElectronicNets::const_iterator net = this->netlist.begin(); net != this->netlist.end(); ++net) {
		Pinlist* pinList = (*net)->getPinList();
		for (Pinlist::const_iterator netPinA = pinList->begin(); netPinA != pinList->end(); ++netPinA) {
			// iterate over parts an match part name and pin name
			for (ElectronicParts::const_iterator partA = this->partlist.begin(); partA != this->partlist.end(); ++partA) {
				if((*partA)->isPlaced() && (netPinA->part == (*partA)->getName())) {
					// first rubberband endpoint
					for (Pinlist::const_iterator netPinB = netPinA+1; netPinB != pinList->end(); ++netPinB) {
						for (ElectronicParts::const_iterator partB = this->partlist.begin(); partB != this->partlist.end(); ++partB) {
							if((*partB)->isPlaced() && (netPinB->part == (*partB)->getName())) {
								// check whether this pad really exists for the parts.
								// getAbsPadPosition returns a [0,0,0] point if the pad doesn't exist.
								if((*partA)->hasPad(netPinA->pin) && (*partB)->hasPad(netPinB->pin)) {
									// at least one part must be visible
									if((*partA)->isVisible() || (*partB)->isVisible()) {
										RubberBand* rb = new RubberBand();
										rb->a = (*partA)->getAbsPadPosition(netPinA->pin);
										rb->b = (*partB)->getAbsPadPosition(netPinB->pin);
										this->rubberBands.push_back(rb);
									}
								}else{
									std::cout << "Net " << (*net)->getName() << " has No matching pad for part " << netPinA->part << " or " << netPinB->part << std::endl;
								}
							}
						}
					}
				}
			}
		}
	}
	return &this->rubberBands;
}

}
