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
			break;
		}
	}
	return result;
}

ElectronicPart* Schematic::getElectronicPart(std::string partName)
{
	ElectronicPart* result;
	for (ElectronicParts::const_iterator part = this->partlist.begin(); part != this->partlist.end(); ++part) {
		if((*part)->getName() == partName) {
			result = (*part);
			break;
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
	Polylines pls_akku;

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
			pls_akku.push_back(pl);
		}
	}

	if(pls_akku.size() > 0) {
		// find longest path for this layer and apply higher overlapping
		Polylines::iterator max_len_pl;
		double max_len = 0;
		for (Polylines::iterator pl = pls_akku.begin(); pl != pls_akku.end(); ++pl) {
			if(pl->length() > max_len) {
				max_len = pl->length();
				max_len_pl = pl;
			}
		}
		// move longest line to front
		Polyline longest_line = (*max_len_pl);
		pls_akku.erase(max_len_pl);
		pls_akku.insert(pls_akku.begin(), longest_line);


		// split paths to equal length parts with small overlap to have extrusion ending at endpoint
		for (Polylines::iterator pl = pls_akku.begin(); pl != pls_akku.end(); ++pl) {
			double clip_length;
			if(pl == pls_akku.begin()) {
				clip_length = (*pl).length()/2 - extrusion_overlap*2; // first extrusion at this layer
			}else{
				clip_length = (*pl).length()/2 - extrusion_overlap;
			}
			Polyline pl2 = (*pl);

			//first half
			(*pl).clip_end(clip_length);
			(*pl).reverse();
			(*pl).remove_duplicate_points();
			if((*pl).length() > scale_(0.05)) {
				pls.push_back((*pl));
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


/* Save placing and routing information to 3de file
 * in XML notation.
 * filebase is used as a reference to the schematic
 * base file in the XML document.
 */
bool Schematic::write3deFile(std::string filename, std::string filebase) {
	pugi::xml_document doc;
	doc.append_attribute("encoding") = "UTF-8";

	pugi::xml_node root_node = doc.append_child("electronics");
	root_node.append_attribute("version") = "0.1";

	pugi::xml_node file_node = root_node.append_child("filename");
	file_node.append_attribute("source") = filebase.c_str();

	//Export electronic components
	pugi::xml_node parts_node = root_node.append_child("parts");
	for (ElectronicParts::const_iterator part = this->partlist.begin(); part != this->partlist.end(); ++part) {
		pugi::xml_node part_node = parts_node.append_child("part");
		part_node.append_attribute("name") = (*part)->getName().c_str();
		part_node.append_attribute("library") = (*part)->getLibrary().c_str();
		part_node.append_attribute("deviceset") = (*part)->getDeviceset().c_str();
		part_node.append_attribute("device") = (*part)->getDevice().c_str();
		part_node.append_attribute("package") = (*part)->getPackage().c_str();

		pugi::xml_node attributes_node = part_node.append_child("attributes");
		attributes_node.append_attribute("partHeight") = (*part)->getPartHeight();
		attributes_node.append_attribute("footprintHeight") = (*part)->getFootprintHeight();
		attributes_node.append_attribute("placed") = (*part)->isPlaced();

		pugi::xml_node pos_node = part_node.append_child("position");
		Pointf3 pos = (*part)->getPosition();
		pos_node.append_attribute("X") = pos.x;
		pos_node.append_attribute("Y") = pos.y;
		pos_node.append_attribute("Z") = pos.z;

		pugi::xml_node rot_node = part_node.append_child("rotation");
		Pointf3 rot = (*part)->getRotation();
		rot_node.append_attribute("X") = rot.x;
		rot_node.append_attribute("Y") = rot.y;
		rot_node.append_attribute("Z") = rot.z;
	}

	// Export netpoints and connections
	pugi::xml_node nets_node = root_node.append_child("nets");
	for (ElectronicNets::const_iterator net = this->netlist.begin(); net != this->netlist.end(); ++net) {
		pugi::xml_node net_node = nets_node.append_child("net");
		net_node.append_attribute("name") = (*net)->getName().c_str();

		// waypoints
		pugi::xml_node waypoints_node = net_node.append_child("waypoints");
		for (std::map<unsigned int, NetPoint>::iterator netPoint = (*net)->netPoints.begin(); netPoint != (*net)->netPoints.end(); ++netPoint) {
			pugi::xml_node waypoint_node = waypoints_node.append_child("waypoint");
			waypoint_node.append_attribute("key") = netPoint->second.getKey();

			pugi::xml_node pos_node = waypoint_node.append_child("position");
			const Pointf3 *pos = netPoint->second.getPoint();
			pos_node.append_attribute("X") = pos->x;
			pos_node.append_attribute("Y") = pos->y;
			pos_node.append_attribute("Z") = pos->z;
		}

		//connections
		pugi::xml_node wires_node = net_node.append_child("wires");
		for (RubberBandPtrs::const_iterator rubberband = (*net)->wiredRubberBands.begin(); rubberband != (*net)->wiredRubberBands.end(); ++rubberband) {
			pugi::xml_node wire_node = wires_node.append_child("wire");

			// first endpoint
			pugi::xml_node endpoint_a_node = wire_node.append_child("endpoint_a");
			if((*rubberband)->hasPartA()) {
				ElectronicPart* partA = this->getElectronicPart((*rubberband)->getPartAiD());
				endpoint_a_node.append_attribute("type") = "part";
				pugi::xml_node endpoint_a_part = endpoint_a_node.append_child("part");
				endpoint_a_part.append_attribute("name") = partA->getName().c_str();
				endpoint_a_part.append_attribute("pin") = (*net)->netPins[(*rubberband)->getNetPinAiD()].pin.c_str();
			}
			if((*rubberband)->hasNetPointA()) {
				endpoint_a_node.append_attribute("type") = "waypoint";
				pugi::xml_node endpoint_a_waypoint = endpoint_a_node.append_child("waypoint");
				endpoint_a_waypoint.append_attribute("key") = (*rubberband)->getNetPointAiD();
			}

			// second endpoint
			pugi::xml_node endpoint_b_node = wire_node.append_child("endpoint_b");
			if((*rubberband)->hasPartB()) {
				ElectronicPart* partB = this->getElectronicPart((*rubberband)->getPartBiD());
				endpoint_b_node.append_attribute("type") = "part";
				pugi::xml_node endpoint_b_part = endpoint_b_node.append_child("part");
				endpoint_b_part.append_attribute("name") = partB->getName().c_str();
				endpoint_b_part.append_attribute("pin") = (*net)->netPins[(*rubberband)->getNetPinBiD()].pin.c_str();
			}
			if((*rubberband)->hasNetPointB()) {
				endpoint_b_node.append_attribute("type") = "waypoint";
				pugi::xml_node endpoint_b_waypoint = endpoint_b_node.append_child("waypoint");
				endpoint_b_waypoint.append_attribute("key") = (*rubberband)->getNetPointBiD();
			}
		}
	}

	bool result = doc.save_file(filename.c_str());
	return result;
}

/* Load placing and routing information from 3de file
 * in XML notation. The actual electronic schematic
 * will be imported from the corresponding file.
 */
bool Schematic::load3deFile(std::string filename) {
	bool result = false;
	pugi::xml_document doc;
	if (!doc.load_file(filename.c_str())) return false;

	// import components
	pugi::xml_node parts = doc.child("electronics").child("parts");
	for (pugi::xml_node part_node = parts.child("part"); part_node; part_node = part_node.next_sibling("part")) {
		std::string name = part_node.attribute("name").value();
		std::string library = part_node.attribute("library").value();
		std::string deviceset = part_node.attribute("deviceset").value();
		std::string device = part_node.attribute("device").value();
		std::string package = part_node.attribute("package").value();

		for (ElectronicParts::iterator part = this->partlist.begin(); part != this->partlist.end(); ++part) {
			if (((*part)->getName() == name) &&
			((*part)->getLibrary() == library) &&
			((*part)->getDeviceset() == deviceset) &&
			((*part)->getDevice() == device) &&
			((*part)->getPackage() == package)) {
				pugi::xml_node attributes_node = part_node.child("attributes");
				(*part)->setPartHeight(attributes_node.attribute("partHeight").as_double());
				(*part)->setFootprintHeight(attributes_node.attribute("footprintHeight").as_double());
				(*part)->setPlaced(attributes_node.attribute("placed").as_bool());

				pugi::xml_node position_node = part_node.child("position");
				(*part)->setPosition(position_node.attribute("X").as_double(),
						position_node.attribute("Y").as_double(),
						position_node.attribute("Z").as_double());

				pugi::xml_node rotation_node = part_node.child("rotation");
				(*part)->setRotation(rotation_node.attribute("X").as_double(),
						rotation_node.attribute("Y").as_double(),
						rotation_node.attribute("Z").as_double());

			}
		}
	}

	// import nets
	pugi::xml_node nets = doc.child("electronics").child("nets");
	for (pugi::xml_node net_node = nets.child("net"); net_node; net_node = net_node.next_sibling("net")) {
		// find corresponding net
		for (ElectronicNets::iterator net = this->netlist.begin(); net != this->netlist.end(); ++net) {
			if((*net)->getName() == net_node.attribute("name").value()) {

				// import waypoints
				pugi::xml_node waypoints = net_node.child("waypoints");
				for (pugi::xml_node waypoint_node = waypoints.child("waypoint"); waypoint_node; waypoint_node = waypoint_node.next_sibling("waypoint")) {
					unsigned int key = waypoint_node.attribute("key").as_uint();
					pugi::xml_node position_node = waypoint_node.child("position");
					Pointf3 pos(position_node.attribute("X").as_double(),
							position_node.attribute("Y").as_double(),
							position_node.attribute("Z").as_double());
					(*net)->netPoints[key] = NetPoint(key, (*net)->getName(), pos);
					(*net)->currentNetPoint = std::max((*net)->currentNetPoint, key);
				}

				// import wires
				pugi::xml_node wires = net_node.child("wires");
				for (pugi::xml_node wire_node = wires.child("wire"); wire_node; wire_node = wire_node.next_sibling("wire")) {
					RubberBand* rb = new RubberBand((*net)->getName(), Pointf3(), Pointf3());

					pugi::xml_node endpoint_a = wire_node.child("endpoint_a");
					if(std::string(endpoint_a.attribute("type").value()) == std::string("waypoint")) {
						rb->addNetPointA(endpoint_a.child("waypoint").attribute("key").as_uint());
					}else if(std::string(endpoint_a.attribute("type").value()) == std::string("part")) {
						std::cout << "type part " << std::endl;
						unsigned int partID = this->getElectronicPart(endpoint_a.child("part").attribute("name").value())->getPartID();
						long netPinID = (*net)->findNetPin(endpoint_a.child("part").attribute("name").value(), endpoint_a.child("part").attribute("pin").value());
						rb->addPartA(partID, netPinID);
					}

					pugi::xml_node endpoint_b = wire_node.child("endpoint_b");
					if(std::string(endpoint_b.attribute("type").value()) == std::string("waypoint")) {
						rb->addNetPointB(endpoint_b.child("waypoint").attribute("key").as_uint());
					}else if(std::string(endpoint_b.attribute("type").value()) == std::string("part")) {
						unsigned int partID = this->getElectronicPart(endpoint_b.child("part").attribute("name").value())->getPartID();
						long netPinID = (*net)->findNetPin(endpoint_b.child("part").attribute("name").value(), endpoint_b.child("part").attribute("pin").value());
						rb->addPartB(partID, netPinID);
					}

					if(!(*net)->addWiredRubberBand(rb)) {
						std::cout << "WARNING: unable to add rubberband to net " << (*net)->getName() << std::endl;
						result = false;
					}
				}
			}
			break;
		}
	}
	this->_updateWiredRubberbands();
	return result;
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
	// update positions of rubberbands
	for (RubberBandPtrs::const_iterator rubberband = net->wiredRubberBands.begin(); rubberband != net->wiredRubberBands.end(); ++rubberband) {
		if((*rubberband)->hasPartA()) {
			ElectronicPart* partA = this->getElectronicPart((*rubberband)->getPartAiD());
			(*rubberband)->a = partA->getAbsPadPosition(net->netPins[(*rubberband)->getNetPinAiD()].pin);
		}
		if((*rubberband)->hasNetPointA()) {
			(*rubberband)->a = (*net->netPoints[(*rubberband)->getNetPointAiD()].getPoint());
		}

		if((*rubberband)->hasPartB()) {
			ElectronicPart* partB = this->getElectronicPart((*rubberband)->getPartBiD());
			(*rubberband)->b = partB->getAbsPadPosition(net->netPins[(*rubberband)->getNetPinBiD()].pin);
		}
		if((*rubberband)->hasNetPointB()) {
			(*rubberband)->b = (*net->netPoints[(*rubberband)->getNetPointBiD()].getPoint());
		}
	}
}

}
