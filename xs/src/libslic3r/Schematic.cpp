#include "Schematic.hpp"

namespace Slic3r {


// electronic parts are placed with respect to the objects bounding box center but the object
// uses the bounding box min point as origin, so we need to translate them.
Schematic::Schematic(const Point objectCenter) : objectCenter(objectCenter)
{
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
			if((*part)->getName() == netPin->part) {
				if((*part)->hasPad(netPin->pin)) {
					netPin->partID = (*part)->getPartID();
					break;
				}else{
					std::cout << "Warning: found matching part for pin  " << netPin->pin << " but no matching pin. Part Info:"<< std::endl;
					(*part)->printPartInfo();
				}
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

void Schematic::setFilename(std::string filename)
{
	this->filename = filename;
}

RubberBandPtrs* Schematic::getRubberBands()
{
	this->rubberBands.clear();
	this->_updateUnwiredRubberbands();
	this->_updateWiredRubberbands();

	RubberBandPtrs::iterator it;
	for (ElectronicNets::const_iterator net = this->netlist.begin(); net != this->netlist.end(); ++net) {
		for (RubberBandPtrs::const_iterator rb = (*net)->unwiredRubberBands.begin(); rb != (*net)->unwiredRubberBands.end(); ++rb) {
			//if(this->_checkRubberBandVisibility((*rb), z)) {
				this->rubberBands.push_back((*rb));
			//}
		}
		for (RubberBandPtrs::const_iterator wrb = (*net)->wiredRubberBands.begin(); wrb != (*net)->wiredRubberBands.end(); ++wrb) {
			//if(this->_checkRubberBandVisibility((*wrb), z)) {
				this->rubberBands.push_back((*wrb));
			//}
		}
	}
	return &this->rubberBands;
}

NetPointPtrs* Schematic::getNetPoints(){
	this->netPoints.clear();

	for (ElectronicNets::const_iterator net = this->netlist.begin(); net != this->netlist.end(); ++net) {
		for (std::map<unsigned int, NetPoint>::iterator netPoint = (*net)->netPoints.begin(); netPoint != (*net)->netPoints.end(); ++netPoint) {
			this->netPoints.push_back(&netPoint->second);
		}
	}
	return &this->netPoints;
}

/*
 * Splits a rubberband into two wired parts. Can be used to wire connections.
 */
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

bool Schematic::removeWire(const unsigned int rubberBandID)
{
	bool result = false;
	// find corresponding net(s)
	for (ElectronicNets::const_iterator net = this->netlist.begin(); net != this->netlist.end(); ++net) {
		result |= (*net)->removeWiredRubberBand(rubberBandID);
		if(result) break;
	}
	if(!result) {
		std::cout << "Warning! failed to remove wire " << rubberBandID << std::endl;
	}
	return result;
}

bool Schematic::removeNetPoint(const NetPoint* netPoint)
{
	bool result = false;
	// find corresponding net(s)
	for (ElectronicNets::const_iterator net = this->netlist.begin(); net != this->netlist.end(); ++net) {
		if(netPoint->getNetName() == (*net)->getName()) {
			result = (*net)->removeNetPoint(netPoint->getKey());
			break;
		}
	}
	if(!result) {
		std::cout << "Warning! failed to remove netPoint " << netPoint->getKey() << std::endl;
	}
	return result;
}

Polylines Schematic::getChannels(const double z_bottom, const double z_top, coord_t extrusion_overlap, coord_t layer_overlap)
{
	Polylines pls;

	for (ElectronicNets::const_iterator net = this->netlist.begin(); net != this->netlist.end(); ++net) {

		// collect all rubberbands for this net
		std::list<Line> lines;
		for (RubberBandPtrs::const_iterator rb = (*net)->wiredRubberBands.begin(); rb != (*net)->wiredRubberBands.end(); ++rb) {
			Line l;
			if((*rb)->getLayerSegment(z_bottom, z_top, layer_overlap, &l)) {
				lines.push_back(l);
			}
		}

		// find connected subsets
		while(lines.size() > 0) {
			Polyline pl;
			std::list<Line>::const_iterator line2;
			// find an endpoint
			for (std::list<Line>::iterator line1 = lines.begin(); line1 != lines.end(); ++line1) {
				int hitsA = 0;
				int hitsB = 0;
				for (std::list<Line>::iterator line2 = lines.begin(); line2 != lines.end(); ++line2) {
					if(line1 == line2) {
						continue;
					}
					if(line1->a.coincides_with_epsilon(line2->a) || line1->a.coincides_with_epsilon(line2->b)) {
						hitsA++;
					}
					if(line1->b.coincides_with_epsilon(line2->a) || line1->b.coincides_with_epsilon(line2->b)) {
						hitsB++;
					}
				}
				if(hitsA == 0) {
					pl.append(line1->a);
					pl.append(line1->b);
					lines.erase(line1);
					break;
				}
				if(hitsB == 0) {
					pl.append(line1->b);
					pl.append(line1->a);
					lines.erase(line1);
					break;
				}
			}

			// we now have an endpoint, traverse lines
			int hits = 1;
			while(hits == 1) {
				hits = 0;
				Point p = pl.last_point();
				std::list<Line>::iterator stored_line;
				Point stored_point;
				for (std::list<Line>::iterator line = lines.begin(); line != lines.end(); ++line) {
					if(p.coincides_with_epsilon(line->a)) {
						hits++;
						if(hits == 1) {
							stored_point = line->b;
							stored_line = line;
							continue;
						}
					}
					if(p.coincides_with_epsilon(line->b)) {
						hits++;
						if(hits == 1) {
							stored_point = line->a;
							stored_line = line;
							continue;
						}
					}
				}
				if(hits == 1) {
					pl.append(stored_point);
					lines.erase(stored_line);
				}
			}
			// split this path to equal length parts with small overlap to have extrusion ending at endpoint
			double clip_length = pl.length()/2 - extrusion_overlap;
			Polyline pl2 = pl;

			//first half
			pl.clip_end(clip_length);
			pl.reverse();
			pl.remove_duplicate_points();
			if(pl.length() > scale_(0.05)) {
				pls.push_back(pl);
			}

			//second half
			pl2.clip_start(clip_length);
			pl2.remove_duplicate_points();
			if(pl2.length() > scale_(0.05)) {
				pls.push_back(pl2);
			}
		}
	}

	// translate to objects origin
	for (Polylines::iterator pl = pls.begin(); pl != pls.end(); ++pl) {
		pl->translate(this->objectCenter.x, this->objectCenter.y);
	}

	return pls;
}

/*
bool Schematic::_checkRubberBandVisibility(const RubberBand* rb, const double z)
{
	bool display = false;
	if(rb->hasPartA()) {
		if(this->getElectronicPart(rb->getPartAiD())->isVisible()) {
			display = true;
		}
	}
	if(rb->hasPartB()) {
		if(this->getElectronicPart(rb->getPartBiD())->isVisible()) {
			display = true;
		}
	}

	if(rb->a.z <= z || rb->b.z <= z) {
		display = true;
	}

	return display;
}
*/

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
				unsigned int netPointBiD = net->findNearestNetPoint(pointA);
				//create and store rubberband
				Pointf3 pointB = *net->netPoints[netPointBiD].getPoint();
				RubberBand* rb = new RubberBand(net->getName(), pointA, pointB);
				rb->addPartA(partA->getPartID(), netPinA);
				rb->addNetPointB(netPointBiD);
				net->unwiredRubberBands.push_back(rb);
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

// update all nets
void Schematic::_updateWiredRubberbands()
{
	for (ElectronicNets::const_iterator net = this->netlist.begin(); net != this->netlist.end(); ++net) {
		this->_updateWiredRubberbands((*net));
	}
}

// update given net
void Schematic::_updateWiredRubberbands(ElectronicNet* net)
{
	// update positions of rubberbands connected to parts
	for (RubberBandPtrs::const_iterator rubberband = net->wiredRubberBands.begin(); rubberband != net->wiredRubberBands.end(); ++rubberband) {
		if((*rubberband)->hasPartA()) {
			ElectronicPart* partA = this->getElectronicPart((*rubberband)->getPartAiD());
			(*rubberband)->a = partA->getAbsPadPosition(net->netPins[(*rubberband)->getNetPinAiD()].pin);
		}
		if((*rubberband)->hasPartB()) {
			ElectronicPart* partB = this->getElectronicPart((*rubberband)->getPartBiD());
			(*rubberband)->b = partB->getAbsPadPosition(net->netPins[(*rubberband)->getNetPinBiD()].pin);
		}
	}
}

}
