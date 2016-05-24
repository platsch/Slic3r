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
	_updateUnwiredRubberbands();
	return this->partlist.back();
}

ElectronicPart* Schematic::getElectronicPart(unsigned int partID)
{
	ElectronicPart* result;
	for (ElectronicParts::const_iterator part = this->partlist.begin(); part != this->partlist.end(); ++part) {
		if((*part)->getPartID() == partID) {
			result = (*part);
		}
	}
	return result;
}

void Schematic::addElectronicNet(ElectronicNet* net)
{
	this->netlist.push_back(net);

	//update part IDs
	for (Pinlist::iterator netPin = net->netPins.begin(); netPin != net->netPins.end(); ++netPin) {
		for (ElectronicParts::const_iterator part = this->partlist.begin(); part != this->partlist.end(); ++part) {
			if((*part)->getName() == netPin->part && (*part)->hasPad(netPin->pin)) {
				netPin->partID = (*part)->getPartID();
				break;
			}
		}
		if(netPin->partID == 0) {
			std::cout << "Warning: unable to locate matching part for " << netPin->part << ", " << netPin->pin << std::endl;
		}
	}
	_updateUnwiredRubberbands(net);
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
	this->rubberBands.clear();
	this->_updateUnwiredRubberbands();

	RubberBandPtrs::iterator it;
	for (ElectronicNets::const_iterator net = this->netlist.begin(); net != this->netlist.end(); ++net) {
		for (RubberBandPtrs::const_iterator rb = (*net)->unwiredRubberBands.begin(); rb != (*net)->unwiredRubberBands.end(); ++rb) {
			if(this->_checkRubberBandVisibility((*rb), 0)) {
				this->rubberBands.push_back((*rb));
			}
		}
		for (RubberBandPtrs::const_iterator wrb = (*net)->wiredRubberBands.begin(); wrb != (*net)->wiredRubberBands.end(); ++wrb) {
			if(this->_checkRubberBandVisibility((*wrb), 0)) {
				this->rubberBands.push_back((*wrb));
			}
		}
	}
	return &this->rubberBands;
}

bool Schematic::_checkRubberBandVisibility(const RubberBand* rb, const double z)
{
	bool display = false;
	if(rb->hasPartA()) {
		if(this->getElectronicPart(rb->getPartAiD())->isVisible()) {
			display = true;
		}
	}if(rb->hasPartB()) {
		if(this->getElectronicPart(rb->getPartBiD())->isVisible()) {
			display = true;
		}
	}

	// wires not connected to a part?? by height?

	return display;
}

void Schematic::splitWire(const RubberBand* rubberband, const Pointf3& p)
{
	// find corresponding net
	for (ElectronicNets::const_iterator net = this->netlist.begin(); net != this->netlist.end(); ++net) {
		if((*net)->getName() == rubberband->getNetName()) {
			// create new netPoint
			unsigned int netPoint = (*net)->addNetPoint(p);

			// first wire
			RubberBand* rbA = new RubberBand(rubberband->getNetName(), rubberband->a, p);
			if(rubberband->hasPartA()) {
				rbA->addPartA(rubberband->getPartAiD(), rubberband->getNetPinAiD());
			}else{
				rbA->addNetPointA(rubberband->getNetPointAiD());
			}

			rbA->addNetPointB(netPoint);
			if (!(*net)->addWiredRubberBand(rbA)) {
				std::cout << "Warning! failed to add netWireA" << std::endl;
				//std::cout << "a.pinA: " << wireA.pinA << " a.pointA: " << wireA.pointA << " a.pinB: " << wireA.pinB << " a.pointB: " << wireA.pointB << std::endl;
			}

			// second wire
			RubberBand* rbB = new RubberBand(rubberband->getNetName(), p, rubberband->b);
			if(rubberband->hasPartB()) {
				rbB->addPartB(rubberband->getPartBiD(), rubberband->getNetPinBiD());
			}else{
				rbB->addNetPointB(rubberband->getNetPointBiD());
			}

			rbB->addNetPointA(netPoint);
			if (!(*net)->addWiredRubberBand(rbB)) {
				std::cout << "Warning! failed to add netWireB" << std::endl;
				//std::cout << "b.pinA: " << wireB.pinA << " b.pointA: " << wireB.pointA << " b.pinB: " << wireB.pinB << " b.pointB: " << wireB.pointB << std::endl;
			}

			//remove original rubberband??
			if(rubberband->isWired()) {
				(*net)->removeWiredRubberBand(rubberband->getID());
			}

			break;
		}
	}
}

// update all nets
void Schematic::_updateUnwiredRubberbands()
{
	for (ElectronicNets::const_iterator net = this->netlist.begin(); net != this->netlist.end(); ++net) {
		this->_updateUnwiredRubberbands((*net));
	}
}

// update given net
void Schematic::_updateUnwiredRubberbands(ElectronicNet* net)
{
	net->unwiredRubberBands.clear();

	//connections between a part and the rest of the net
	for (int netPinA = 0; netPinA < net->netPins.size(); netPinA++) {
		if(net->netPins[netPinA].partID < 1) {continue;} // real pin?

		//locate part A
		ElectronicPart* partA = this->getElectronicPart(net->netPins[netPinA].partID);
		if(!partA->isPlaced()) {continue;} // part already placed?
		Pointf3 pointA = partA->getAbsPadPosition(net->netPins[netPinA].pin);

		//is this pin connected?
		bool connected = false;
		for (RubberBandPtrs::const_iterator rubberband = net->wiredRubberBands.begin(); rubberband != net->wiredRubberBands.end(); ++rubberband) {
			if((*rubberband)->connectsNetPin(netPinA)) {
				connected = true;
				break;
			}
		}
		// partID > 0 implies this part actually exists
		if(!connected) {
			//find nearest point
			if(net->wiredRubberBands.size() > 0) {
				net->findNearestNetPoint(pointA);
				//create and store rubberband here
			}else{ // find nearest part
				if(netPinA < net->netPins.size()) {
					// iterate over remaining netPins

					ElectronicPart* partB;
					Pointf3 pointB;
					unsigned int minPin = 0;
					double minDist = 9999999999999.0;
					for (int netPinB = netPinA; netPinB < net->netPins.size(); netPinB++) {
						if(net->netPins[netPinB].partID > 0 && net->netPins[netPinB].partID != net->netPins[netPinA].partID) {
							partB = this->getElectronicPart(net->netPins[netPinB].partID);
							if(!partB->isPlaced()) {continue;}
							pointB = partB->getAbsPadPosition(net->netPins[netPinB].pin);
							double dist = pointA.distance_to(pointB);
							if(dist < minDist) {
								minDist = dist;
								minPin = netPinB;
							}
						}
					}
					if(minPin > 0) {
						//create and store rubberband
						partB = this->getElectronicPart(net->netPins[minPin].partID);
						pointB = partB->getAbsPadPosition(net->netPins[minPin].pin);

						RubberBand* rb = new RubberBand(net->getName(), pointA, pointB);
						rb->addPartA(partA->getPartID(), netPinA);
						rb->addPartB(partB->getPartID(), minPin);
						net->unwiredRubberBands.push_back(rb);
					}
				}
			}
		}

	}

	//connections between two separate net segments
}

}
