/* The topology of each net is stored in a boost::graph object
 * with a builtin index vertex_index_t (netGraphIndex) for references
 * to the netPoints and an external index (netPointIndex) for reverse lookup.
 * Each vertex represents a waypoint for a conductive trace.
 * The graph library is used to find "connected components" to generate
 * rubberbands for currently unwired connections.
 * Probably there will be more features requiring graph algorithms in the future.
 */

#include "ElectronicNet.hpp"

namespace Slic3r {


/* Generates a new ElectronicNet.
 * netPoint IDs start with 1, ID = 0 -> undefined.
 */
ElectronicNet::ElectronicNet(std::string name)
{
    this->name = name;
    this->currentNetPoint = 0;

    // assign graph vertex index
    this->netGraphIndex = get(boost::vertex_index, this->netGraph);
}

ElectronicNet::~ElectronicNet()
{
}

std::string ElectronicNet::getName()
{
    return this->name;
}

void ElectronicNet::addPin(std::string part, std::string pin, std::string gate)
{
    ElectronicNetPin netPin;
    netPin.part = part;
    netPin.pin = pin;
    netPin.gate = gate;
    netPin.partID = 0;
    netPin.netPointKey = 0;
    this->netPins.push_back(netPin);
}

Pinlist* ElectronicNet::getPinList()
{
    return &this->netPins;
}

/* Find a netPin by part and pin name.
 * Returns a pointer to the pin or NULL otherwise.
 */
ElectronicNetPin* ElectronicNet::findNetPin(const std::string partName, const std::string pinName)
{
    ElectronicNetPin* result = NULL;
    for (int i = 0; i < this->netPins.size(); i++) {
        if(this->netPins[i].part == partName && this->netPins[i].pin == pinName) {
            result = &this->netPins[i];
            break;
        }
    }
    return result;
}

/* Find a netPin by key of the corresponding netPoint.
 * Returns a pointer to the pin or NULL otherwise.
 */
ElectronicNetPin* ElectronicNet::findNetPin(const unsigned int netPointKey) {
    ElectronicNetPin* result = NULL;
    for (int i = 0; i < this->netPins.size(); i++) {
        if(this->netPins[i].netPointKey == netPointKey) {
            result = &this->netPins[i];
            break;
        }
    }
    return result;
}

unsigned int ElectronicNet::addNetPoint(const netPointType type, Pointf3 p)
{
    this->currentNetPoint++;
    this->netPoints[this->currentNetPoint] = NetPoint(this->currentNetPoint, type, this->name, p); // create netPoint object
    vertex_t v = boost::add_vertex(this->currentNetPoint, this->netGraph); // add vertex to graph
    this->netPointIndex[this->currentNetPoint] = v;

    return this->currentNetPoint;
}

void ElectronicNet::updateNetPoint(const unsigned int netPointKey, const netPointType type, const Pointf3 p)
{
    // netPoint still exists?
    if(this->netPoints.find(netPointKey) == this->netPoints.end()) {
        this->netPoints[netPointKey] = NetPoint(netPointKey, type, this->name, p);
        vertex_t v = boost::add_vertex(netPointKey, this->netGraph);
        this->netPointIndex[netPointKey] = v;
        // make sure the current ID is still valid
        this->currentNetPoint = std::max(this->currentNetPoint, netPointKey);
    }else{
        this->netPoints[netPointKey].point = p;
    }
}

bool ElectronicNet::removeNetPoint(unsigned int netPointKey)
{
    bool result = false;
    if(this->netPoints.erase(netPointKey) > 0) {
        boost::clear_vertex(netPointIndex[netPointKey], this->netGraph);
        boost::remove_vertex(netPointIndex[netPointKey], this->netGraph);
        netPointIndex.erase(netPointKey);
        result = true;
    }

    return result;
}

bool ElectronicNet::addWire(const unsigned int netPointAiD, const unsigned int netPointBiD)
{
    bool result = false;
    // check if both points exist
    if((this->netPoints.find(netPointAiD) != this->netPoints.end()) && (this->netPoints.find(netPointBiD) != this->netPoints.end())) {
        boost::add_edge(this->netPointIndex[netPointAiD], this->netPointIndex[netPointBiD], this->netGraph);
        result = true;
    }

    return result;
}

/* Removes given connection from netGraph and clears abandoned netPoints
 * returns false if this connection doesn't exist
 */
bool ElectronicNet::removeWire(const unsigned int netPointAiD, const unsigned int netPointBiD)
{
    bool result = false;
    // check if both points exist
    if((this->netPoints.find(netPointAiD) != this->netPoints.end()) && (this->netPoints.find(netPointBiD) != this->netPoints.end())) {
        vertex_t a = this->netPointIndex[netPointAiD];
        vertex_t b = this->netPointIndex[netPointBiD];
        boost::remove_edge(a, b, this->netGraph);

        // check for abandoned netPoints
        if(boost::degree(a, this->netGraph) < 1) {
            this->removeNetPoint(netPointAiD);

        }
        if(boost::degree(b, this->netGraph) < 1) {
            this->removeNetPoint(netPointBiD);
        }

        result = true;
    }
    return result;
}

// Delete corresponding connection from the netGraph
// and remove rubberBand with given ID from the vector.
// returns true if a matching RB exists, false otherwise
bool ElectronicNet::removeWiredRubberBand(const unsigned int ID)
{
    bool result = false;
    for (RubberBandPtrs::iterator rb = this->wiredRubberBands.begin(); rb != this->wiredRubberBands.end(); ++rb) {
        if((*rb)->getID() == ID) {
            this->removeWire((*rb)->getNetPointAiD(), (*rb)->getNetPointBiD());
            this->wiredRubberBands.erase(rb);
            result = true;
            break;
        }
    }
    return result;
}

const NetPoint* ElectronicNet::findNearestNetPoint(const Pointf3& p) const
{
    const NetPoint* result = NULL;
    double min_dist = 999999999999999.0;
    for (std::map<unsigned int, NetPoint>::const_iterator netPoint = this->netPoints.begin(); netPoint != this->netPoints.end(); ++netPoint) {
        const NetPoint* np = &netPoint->second;
        Pointf3 point = np->getPoint();
        double dist = point.distance_to(p);
        if(dist < min_dist) {
            min_dist = dist;
            result = np;
        }
    }
    return result;
}

/*bool ElectronicNet::_pointIsConnected(unsigned int netPointKey)
{
    bool result = false;
    for (RubberBandPtrs::const_iterator rubberband = this->wiredRubberBands.begin(); rubberband != this->wiredRubberBands.end(); ++rubberband) {
        if((*rubberband)->hasNetPointA()) {
            result |= ((*rubberband)->getNetPointAiD() == netPointKey);
        }
        if((*rubberband)->hasNetPointB()) {
            result |= ((*rubberband)->getNetPointBiD() == netPointKey);
        }
        if(result) break;
    }
    return result;
}*/

/* Check whether the given waypoints are connected
 * (in the same component of the graph)
 */
bool ElectronicNet::waypointsConnected(const unsigned int netPointKeyA, const unsigned int netPointKeyB)
{
    bool result = false;

    // index property map
    std::map<vertex_t, unsigned int> mapIndex;
    boost::associative_property_map< std::map<vertex_t, unsigned int> > componentIndex(mapIndex);

    // color property map
    std::map<vertex_t, boost::default_color_type> colorMap;
    boost::associative_property_map< std::map<vertex_t, boost::default_color_type> > color_prop_map(colorMap);

    // find all connected components
    int componentNum = boost::connected_components(this->netGraph, componentIndex, boost::color_map(color_prop_map));

    vertex_t a = this->netPointIndex[netPointKeyA];
    vertex_t b = this->netPointIndex[netPointKeyB];

    if(componentIndex[a] == componentIndex[b]) {
        result = true;
    }

    return result;
}

/*
 * How many connections are going from / to this waypoint?
 */
const unsigned int ElectronicNet::waypointDegree(const unsigned int waypointID)
{
    vertex_t v = this->netPointIndex[waypointID];
    return boost::degree(v, this->netGraph);
}


/* Generates rubberbands of wired connections
 * from netGraph and returns the result
 */
RubberBandPtrs* ElectronicNet::generateWiredRubberBands()
{
    this->wiredRubberBands.clear();
    // generate rubberBands
    boost::graph_traits<Graph>::edge_iterator ei, ei_end;
    for (boost::tie(ei, ei_end) = edges(this->netGraph); ei != ei_end; ++ei) {

        unsigned int aId = netGraphIndex[boost::source(*ei, this->netGraph)];
        unsigned int bId = netGraphIndex[boost::target(*ei, this->netGraph)];
        RubberBand* rb = new RubberBand(this->name, &this->netPoints[aId], &this->netPoints[bId], waypointDegree(aId), waypointDegree(bId), true);
        this->wiredRubberBands.push_back(rb);
    }

    return &this->wiredRubberBands;
}

/* Generates rubberbands of all currently unwired
 * connections by a depth first search on the connection graph
 * http://www.boost.org/doc/libs/1_61_0/libs/graph/doc/connected_components.html
 */
RubberBandPtrs* ElectronicNet::getUnwiredRubberbands()
{
    this->unwiredRubberBands.clear();

    // update index - this should be unnecessary...
    //this->netGraphIndex = get(boost::vertex_index, this->netGraph);

    // index property map
    std::map<vertex_t, unsigned int> mapIndex;
    boost::associative_property_map< std::map<vertex_t, unsigned int> > componentIndex(mapIndex);

    // color property map
    std::map<vertex_t, boost::default_color_type> colorMap;
    boost::associative_property_map< std::map<vertex_t, boost::default_color_type> > color_prop_map(colorMap);

    // find all connected components
    int componentNum = boost::connected_components(this->netGraph, componentIndex, boost::color_map(color_prop_map));

    // map components to netPointKeys
    std::map<int, std::vector<unsigned int> > componentMap;
    boost::graph_traits<Graph>::vertex_iterator b, e;
    for (boost::tie(b,e) = boost::vertices(this->netGraph); b!=e; ++b) {
        componentMap[componentIndex[*b]].push_back(this->netGraphIndex(*b));
    }

    // iterate through all components and find shortest connections
    for (int i=0; i<componentNum; i++) {
        for (int k=i+1; k<componentNum; k++) {
            Pointf3 iPoint = this->netPoints[*componentMap[i].begin()].getPoint(); // first element of component i
            Pointf3 kPoint = this->netPoints[*componentMap[k].begin()].getPoint(); // first element of component k
            unsigned int netPointA = *componentMap[i].begin();
            unsigned int netPointB = *componentMap[k].begin();
            double min_dist = iPoint.distance_to(kPoint);

            // find shortest distance between this two components
            for (std::vector<unsigned int>::const_iterator iIter = componentMap[i].begin(); iIter != componentMap[i].end(); ++iIter) {
                for (std::vector<unsigned int>::const_iterator kIter = componentMap[k].begin(); kIter != componentMap[k].end(); ++kIter) {
                    iPoint = this->netPoints[*iIter].getPoint();
                    kPoint = this->netPoints[*kIter].getPoint();

                    if(iPoint.distance_to(kPoint) < min_dist) {
                        min_dist = iPoint.distance_to(kPoint);
                        netPointA = *iIter;
                        netPointB = *kIter;
                    }
                }
            }
            RubberBand* rb = new RubberBand(this->getName(), &this->netPoints[netPointA], &this->netPoints[netPointB], waypointDegree(netPointA), waypointDegree(netPointB), false);
            this->unwiredRubberBands.push_back(rb);
        }
    }
    return &this->unwiredRubberBands;
}

}
