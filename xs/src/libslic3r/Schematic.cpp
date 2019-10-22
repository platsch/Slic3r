#include "Schematic.hpp"

namespace Slic3r {


// electronic parts are placed with respect to the objects bounding box center but the object
// uses the bounding box min point as origin, so we need to translate them.
Schematic::Schematic()
{
    this->filename = "Default file";
}

Schematic::Schematic(const Schematic &other)
{
    *this = other;
}

Schematic::~Schematic()
{
}

Schematic& Schematic::operator=(const Schematic &other)
{
    this->netlist = other.netlist;
    this->partlist = other.partlist;
    this->filename = other.filename;
    this->rubberBands = other.rubberBands;
    this->netPoints = other.netPoints;
    return *this;
}

void Schematic::addElectronicPart(ElectronicPart* part)
{
    this->partlist.push_back(part);
    this->updatePartNetPoints(part);
}

ElectronicPart* Schematic::addElectronicPart(std::string name, std::string library, std::string deviceset, std::string device, std::string package)
{
    this->addElectronicPart(new ElectronicPart(name, library, deviceset, device, package));
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
}

ElectronicParts* Schematic::getPartlist()
{
    return &this->partlist;
}

AdditionalParts* Schematic::getAdditionalPartlist()
{
    // this->additionalPartlist.push_back(new AdditionalPart("2.5"));
    // this->additionalPartlist.push_back(new AdditionalPart("3"));
    // this->additionalPartlist.push_back(new AdditionalPart("4"));
    // this->additionalPartlist.push_back(new AdditionalPart("5"));
    this->additionalPartlist.push_back(new AdditionalPart("6"));
    return &this->additionalPartlist;
}

void Schematic::addAdditionalPart(std::string thread_size, std::string type)
{
    this->additionalPartlist.push_back(new AdditionalPart(thread_size));
}


    void
    Schematic::setFilename(std::string filename)
{
    this->filename = filename;
}

// Generate a new set of rubberbands from net graph.
// Previous rubberbands are invalidated by this step since we allocate new IDs!!
void Schematic::updateRubberBands()
{
    this->rubberBands.clear();
    this->updatePartNetPoints();

    for (ElectronicNets::const_iterator net = this->netlist.begin(); net != this->netlist.end(); ++net) {
        // unwired rubberbands
        RubberBandPtrs* rbp = (*net)->getUnwiredRubberbands();
        for (RubberBandPtrs::const_iterator rb = rbp->begin(); rb != rbp->end(); ++rb) {
            this->rubberBands.push_back((*rb));
        }

        // wired rubberbands
        RubberBandPtrs* wiredRbs = (*net)->generateWiredRubberBands();
        for (RubberBandPtrs::const_iterator wrb = wiredRbs->begin(); wrb != wiredRbs->end(); ++wrb) {
            //if(this->_checkRubberBandVisibility((*wrb), z)) {
                this->rubberBands.push_back((*wrb));
            //}
        }
    }
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

/* find the nearest netPoint coordinates from the net indicated by the rubberband
 * relative to p. A call to splitWire should result in a wire ending at the same point.
 * Returns p if no point is within a threshold.
 */
const NetPoint* Schematic::findNearestSplittingPoint(const NetPoint* sourceWayPoint, const Pointf3& p) const
{
    const NetPoint* result = NULL;

    for (ElectronicNets::const_iterator net = this->netlist.begin(); net != this->netlist.end(); ++net) {
        if((*net)->getName() == sourceWayPoint->getNetName()) {
            const NetPoint* np = (*net)->findNearestNetPoint(p);
            if(np) {
                if(p.distance_to(np->getPoint()) < 2) { // check distance
                    if(!(*net)->waypointsConnected(np->getKey(), sourceWayPoint->getKey())) { // check for cycles
                        result = np;
                    }
                }
            }
        }
    }
    return result;
}

/* Add a wire from netPoint to given point.
 * Adds a wayopint or connects to a waypoing within "magnetic" range.
 */
void Schematic::addWire(const NetPoint* netPoint, const Pointf3& p)
{
    // find corresponding net
    for (ElectronicNets::const_iterator net = this->netlist.begin(); net != this->netlist.end(); ++net) {
        if((*net)->getName() == netPoint->getNetName()) {

            unsigned int netPointKey = 0;

            // connecting to an existing netPoint?
            const NetPoint* np = this->findNearestSplittingPoint(netPoint, p);
            if(np) {
                netPointKey = np->getKey();
            }

            // no, create new netPoint
            if(netPointKey == 0) {
                netPointKey = (*net)->addNetPoint(WAYPOINT, p);
            }

            if (!(*net)->addWire(netPoint->getKey(), netPointKey)) {
                std::cout << "Warning! failed to add netWire" << std::endl;
            }

            break;
        }
    }
    this->updateRubberBands();
}


/* Splits a rubberband into two wired parts or
 * a single wire if one point is selected.
 * Can be used to wire connections.
 */
void Schematic::splitWire(const RubberBand* rubberband, const Pointf3& p)
{
    // find corresponding net
    for (ElectronicNets::const_iterator net = this->netlist.begin(); net != this->netlist.end(); ++net) {
        if((*net)->getName() == rubberband->getNetName()) {

            unsigned int netPointKey = 0;

            // connecting to an existing netPoint?
            if(!rubberband->isWired()) {
                const NetPoint* sourceWayPoint = rubberband->pointASelected() ? rubberband->getNetPointA() : rubberband->getNetPointB();
                const NetPoint* np = this->findNearestSplittingPoint(sourceWayPoint, p);
                if(np) {
                    netPointKey = np->getKey();
                }
            }
            // no, create new netPoint
            if(netPointKey == 0) {
                netPointKey = (*net)->addNetPoint(WAYPOINT, p);
            }

            // new wires
            if(!rubberband->pointBSelected()) {
                if (!(*net)->addWire(rubberband->getNetPointAiD(), netPointKey)) {
                    std::cout << "Warning! failed to add netWireA" << std::endl;
                    //std::cout << "a.pinA: " << wireA.pinA << " a.pointA: " << wireA.pointA << " a.pinB: " << wireA.pinB << " a.pointB: " << wireA.pointB << std::endl;
                }
            }
            if(!rubberband->pointASelected()) {
                if (!(*net)->addWire(rubberband->getNetPointBiD(), netPointKey)) {
                    std::cout << "Warning! failed to add netWireB" << std::endl;
                    //std::cout << "b.pinA: " << wireB.pinA << " b.pointA: " << wireB.pointA << " b.pinB: " << wireB.pinB << " b.pointB: " << wireB.pointB << std::endl;
                }
            }

            //remove original wire??
            if(rubberband->isWired()) {
                if(!(*net)->removeWiredRubberBand(rubberband->getID())) {
                    std::cout << "Warning! failed to remove wire " << rubberband->getID() << "  after splitting" << std::endl;
                }
            }

            break;
        }
    }
    this->updateRubberBands();
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
    this->updateRubberBands();
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
    this->updateRubberBands();
    return result;
}

Polylines Schematic::getChannels(const double z_bottom, const double z_top, coord_t extrusion_overlap, coord_t first_extrusion_overlap, coord_t overlap_min_extrusion_length, coord_t layer_overlap)
{
    Polylines pls;
    Polylines pls_akku;

    for (ElectronicNets::const_iterator net = this->netlist.begin(); net != this->netlist.end(); ++net) {
        // collect all rubberbands for this net
        std::list<Line> lines;
        for (RubberBandPtrs::const_iterator rb = (*net)->wiredRubberBands.begin(); rb != (*net)->wiredRubberBands.end(); ++rb) {
            Lines l;
            bool extendPinA = ((*net)->wayointDegree((*rb)->getNetPointAiD()) == 1);
            bool extendPinB = ((*net)->wayointDegree((*rb)->getNetPointBiD()) == 1);
            if((*rb)->getLayerSegments(z_bottom, z_top, layer_overlap, &l, extendPinA, extendPinB)) {
                for (Lines::iterator line = l.begin(); line != l.end(); ++line) {
                    lines.push_back(*line);
                }
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
                clip_length = (*pl).length()/2 - first_extrusion_overlap; // first extrusion at this layer
            }else{
                clip_length = (*pl).length()/2 - extrusion_overlap;
            }

            if((((*pl).length()/2 - clip_length) > EPSILON) && ((*pl).length() > overlap_min_extrusion_length)) {
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
            }else{
                pls.push_back(*pl);
            }
        }
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
        attributes_node.append_attribute("placingMethod") = (*part)->getPlacingMethodString().c_str();
        attributes_node.append_attribute("connectionMethod") = (*part)->getConnectionMethodString().c_str();

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
            if(netPoint->second.getType() == WAYPOINT) {
                pugi::xml_node waypoint_node = waypoints_node.append_child("waypoint");
                waypoint_node.append_attribute("key") = netPoint->second.getKey();

                pugi::xml_node pos_node = waypoint_node.append_child("position");
                const Pointf3 pos = netPoint->second.getPoint();
                pos_node.append_attribute("X") = pos.x;
                pos_node.append_attribute("Y") = pos.y;
                pos_node.append_attribute("Z") = pos.z;
            }
        }

        //connections
        pugi::xml_node wires_node = net_node.append_child("wires");
        RubberBandPtrs *rbs = (*net)->generateWiredRubberBands();
        for (RubberBandPtrs::const_iterator rubberband = rbs->begin(); rubberband != rbs->end(); ++rubberband) {
            pugi::xml_node wire_node = wires_node.append_child("wire");

            // first endpoint
            pugi::xml_node endpoint_a_node = wire_node.append_child("endpoint_a");
            if((*rubberband)->getNetPointA()->getType() == PART) {
                endpoint_a_node.append_attribute("type") = "part";
                pugi::xml_node endpoint_a_part = endpoint_a_node.append_child("part");
                ElectronicNetPin* pin = (*net)->findNetPin((*rubberband)->getNetPointA()->getKey());
                endpoint_a_part.append_attribute("name") = pin->part.c_str();
                endpoint_a_part.append_attribute("pin") = pin->pin.c_str();
            }else{
                endpoint_a_node.append_attribute("type") = "waypoint";
                pugi::xml_node endpoint_a_waypoint = endpoint_a_node.append_child("waypoint");
                endpoint_a_waypoint.append_attribute("key") = (*rubberband)->getNetPointA()->getKey();
            }

            // second endpoint
            pugi::xml_node endpoint_b_node = wire_node.append_child("endpoint_b");
            if((*rubberband)->getNetPointB()->getType() == PART) {
                endpoint_b_node.append_attribute("type") = "part";
                pugi::xml_node endpoint_b_part = endpoint_b_node.append_child("part");
                ElectronicNetPin* pin = (*net)->findNetPin((*rubberband)->getNetPointB()->getKey());
                endpoint_b_part.append_attribute("name") = pin->part.c_str();
                endpoint_b_part.append_attribute("pin") = pin->pin.c_str();
            }else{
                endpoint_b_node.append_attribute("type") = "waypoint";
                pugi::xml_node endpoint_b_waypoint = endpoint_b_node.append_child("waypoint");
                endpoint_b_waypoint.append_attribute("key") = (*rubberband)->getNetPointB()->getKey();
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
                (*part)->setPlacingMethod(attributes_node.attribute("placingMethod").as_string());
                (*part)->setConnectionMethod(attributes_node.attribute("connectionMethod").as_string());

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

    this->updatePartNetPoints();

    // import nets
    pugi::xml_node nets = doc.child("electronics").child("nets");
    for (pugi::xml_node net_node = nets.child("net"); net_node; net_node = net_node.next_sibling("net")) {
        // find corresponding net
        for (ElectronicNets::iterator net = this->netlist.begin(); net != this->netlist.end(); ++net) {
            if((*net)->getName() == net_node.attribute("name").value()) {

                std::map<unsigned int, unsigned int> waypointMapping;

                // import waypoints
                pugi::xml_node waypoints = net_node.child("waypoints");
                for (pugi::xml_node waypoint_node = waypoints.child("waypoint"); waypoint_node; waypoint_node = waypoint_node.next_sibling("waypoint")) {
                    unsigned int key = waypoint_node.attribute("key").as_uint();
                    pugi::xml_node position_node = waypoint_node.child("position");
                    Pointf3 pos(position_node.attribute("X").as_double(),
                            position_node.attribute("Y").as_double(),
                            position_node.attribute("Z").as_double());
                    waypointMapping[key] = (*net)->addNetPoint(WAYPOINT, pos);
                }

                // import wires
                pugi::xml_node wires = net_node.child("wires");
                for (pugi::xml_node wire_node = wires.child("wire"); wire_node; wire_node = wire_node.next_sibling("wire")) {
                    unsigned int netPointA = 0;
                    unsigned int netPointB = 0;

                    pugi::xml_node endpoint_a = wire_node.child("endpoint_a");
                    if(std::string(endpoint_a.attribute("type").value()) == std::string("waypoint")) {
                        netPointA = waypointMapping[endpoint_a.child("waypoint").attribute("key").as_uint()];
                    }else if(std::string(endpoint_a.attribute("type").value()) == std::string("part")) {
                        ElectronicNetPin* pin = (*net)->findNetPin(endpoint_a.child("part").attribute("name").value(), endpoint_a.child("part").attribute("pin").value());
                        if(pin) {
                            netPointA = pin->netPointKey;
                        }
                    }

                    pugi::xml_node endpoint_b = wire_node.child("endpoint_b");
                    if(std::string(endpoint_b.attribute("type").value()) == std::string("waypoint")) {
                        netPointB = waypointMapping[endpoint_b.child("waypoint").attribute("key").as_uint()];
                    }else if(std::string(endpoint_b.attribute("type").value()) == std::string("part")) {
                        ElectronicNetPin* pin = (*net)->findNetPin(endpoint_b.child("part").attribute("name").value(), endpoint_b.child("part").attribute("pin").value());
                        if(pin) {
                            netPointB = pin->netPointKey;
                        }
                    }

                    if(!(*net)->addWire(netPointA, netPointB)) {
                        std::cout << "WARNING: unable to add wire to net " << (*net)->getName() << std::endl;
                        result = false;
                    }
                }
                break;
            }
        }
    }
    this->updateRubberBands();
    return result;
}

void Schematic::updatePartNetPoints()
{
    for (ElectronicParts::const_iterator part = this->partlist.begin(); part != this->partlist.end(); ++part) {
        this->updatePartNetPoints((*part));
    }
}

/* update alle netPoints that
 * correspond to a pin of this part
 */
void Schematic::updatePartNetPoints(ElectronicPart* part)
{
    ElectronicNetPin* pin;
    for (ElectronicNets::const_iterator net = this->netlist.begin(); net != this->netlist.end(); ++net) {
        for (Padlist::const_iterator pad = part->padlist.begin(); pad != part->padlist.end(); ++pad) {
            //if((*net)->findNetPin(part->getName(), (*pad).pin, pin)) {
            pin = (*net)->findNetPin(part->getName(), (*pad).pin);
            if(pin) {
                // update netPoint
                // if part is not placed -> remove netPoint
                if(part->isPlaced()) {
                    if(pin->netPointKey == 0) {
                        // create new point if this point never existed
                        unsigned int netPoint = (*net)->addNetPoint(PART, part->getAbsPadPosition((*pad).pin));
                        pin->netPointKey = netPoint;
                    }else{
                        // update netPoint
                        (*net)->updateNetPoint(pin->netPointKey, PART, part->getAbsPadPosition((*pad).pin));
                    }
                    // update route extension points
                    NetPoint* np = (*net)->getNetPoint(pin->netPointKey);
                    np->setRouteExtensionPoints(part->getAbsPadPositionPerimeter((*pad).pin, true), part->getAbsPadPositionPerimeter((*pad).pin, false));
                }else{
                    (*net)->removeNetPoint(pin->netPointKey);
                }
            }
        }
    }
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
}
