#include "ElectronicNet.hpp"

namespace Slic3r {


/* Generates a new ElectronicNet.
 * netPoint IDs start with 1, ID = 0 -> undefined.
 */
ElectronicNet::ElectronicNet(std::string name)
{
	this->name = name;
	this->currentNetPoint = 0;
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
	netPin.netPointID = 0;
	this->netPins.push_back(netPin);
}

Pinlist* ElectronicNet::getPinList()
{
	return &this->netPins;
}

/* Find a netPin by part and pin name.
 * Returns a pointer to the pin or NULL otherwise.
 *
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

unsigned int ElectronicNet::addNetPoint(const netPointType type, Pointf3 p)
{
	this->currentNetPoint++;
	this->netPoints[this->currentNetPoint] = NetPoint(this->currentNetPoint, type, this->name, p); // create netPoint object
	vertex_t v = boost::add_vertex(this->currentNetPoint, this->netGraph); // add vertex to graph
	this->netPointIndex[this->currentNetPoint] = v;

	return this->currentNetPoint;
}

void ElectronicNet::updateNetPoint(const netPointType type, const unsigned int netPointID, const Pointf3 p)
{
	// netPoint still exists?
	if(this->netPoints.find(netPointID) == this->netPoints.end()) {
		this->netPoints[netPointID] = NetPoint(netPointID, type, this->name, p);
		vertex_t v = boost::add_vertex(netPointID, this->netGraph);
		this->netPointIndex[netPointID] = v;
	}else{
		this->netPoints[netPointID].point = p;
	}
}

bool ElectronicNet::removeNetPoint(unsigned int netPointID)
{
	bool result = false;
	if(this->netPoints.erase(netPointID) > 0) {
		boost::remove_vertex(netPointIndex[netPointID], this->netGraph);
		netPointIndex.erase(netPointID);
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

	std::cout << "add wire from " << netPointAiD << " to " << netPointBiD << std::endl;
	write_graphviz(std::cout, this->netGraph);

	return result;
}

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
			this->netPoints.erase(netPointAiD);
			boost::remove_vertex(a, this->netGraph);
			netPointIndex.erase(netPointAiD);

		}
		if(boost::degree(b, this->netGraph) < 1) {
			this->netPoints.erase(netPointBiD);
			boost::remove_vertex(b, this->netGraph);
			netPointIndex.erase(netPointBiD);
		}

		std::cout << "remove wire from " << netPointAiD << " to " << netPointBiD << std::endl;
		write_graphviz(std::cout, this->netGraph);

		result = true;
	}
	return result;
}


/*bool ElectronicNet::addWiredRubberBand(RubberBand* rb)
{
	bool result = false;

	// check if given points exist and exactly one A and one B are defined
	if(rb->hasPartA() || rb->hasNetPointA()) {
		if(rb->hasPartB() || rb->hasNetPointB()) {
			rb->setWired(true);
			this->wiredRubberBands.push_back(rb);
			result = true;
		}
	}

	return result;
}
*/
// Removes rubberBand with given ID from the vector, returns true if a matching RB exists, false otherwise
bool ElectronicNet::removeWiredRubberBand(const unsigned int ID)
{
	bool result = false;
	for(int i = 0; i < this->wiredRubberBands.size(); i++) {
		if(this->wiredRubberBands[i]->getID() == ID) {
			this->removeWire(this->wiredRubberBands[i]->getNetPointAiD(), this->wiredRubberBands[i]->getNetPointBiD());
			result = true;
			break;
		}
	}
	return result;
}

unsigned int ElectronicNet::findNearestNetPoint(const Pointf3& p) const
{
	unsigned int result = 0;
	double min_dist = 999999999999999.0;
	for (std::map<unsigned int, NetPoint>::const_iterator netPoint = this->netPoints.begin(); netPoint != this->netPoints.end(); ++netPoint) {
		NetPoint np = netPoint->second;
		Pointf3 point = *np.getPoint();
		double dist = point.distance_to(p);
		if(dist < min_dist) {
			min_dist = dist;
			result = netPoint->first;
		}
	}
	return result;
}

/*bool ElectronicNet::_pointIsConnected(unsigned int netPointID)
{
	bool result = false;
	for (RubberBandPtrs::const_iterator rubberband = this->wiredRubberBands.begin(); rubberband != this->wiredRubberBands.end(); ++rubberband) {
		if((*rubberband)->hasNetPointA()) {
			result |= ((*rubberband)->getNetPointAiD() == netPointID);
		}
		if((*rubberband)->hasNetPointB()) {
			result |= ((*rubberband)->getNetPointBiD() == netPointID);
		}
		if(result) break;
	}
	return result;
}*/

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
		RubberBand* rb = new RubberBand(this->name, &this->netPoints[aId], &this->netPoints[bId], true);
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

	// update index
	this->netGraphIndex = get(boost::vertex_index, this->netGraph);

	// find all connected components
	boost::property_map<Graph, boost::vertex_index1_t>::type componentIndex;
	int componentNum = boost::connected_components(this->netGraph, componentIndex);

	// map components to netPointIDs
	std::map<int, std::vector<unsigned int> > componentMap;
	boost::graph_traits<Graph>::vertex_iterator b, e;
	for (boost::tie(b,e) = boost::vertices(this->netGraph); b!=e; ++b) {
		componentMap[componentIndex[*b]].push_back(this->netGraphIndex(*b));
	}

	// iterate through all components and find shortest connections
	for (int i=0; i<componentNum; i++) {
		for (int k=i+1; k<componentNum; k++) {
			Pointf3 iPoint = *this->netPoints[*componentMap[i].begin()].getPoint(); // first element of component i
			Pointf3 kPoint = *this->netPoints[*componentMap[k].begin()].getPoint(); // first element of component k
			unsigned int netPointA = *componentMap[i].begin();
			unsigned int netPointB = *componentMap[k].begin();
			double min_dist = iPoint.distance_to(kPoint);

			// find shortest distance between this two components
			for (std::vector<unsigned int>::const_iterator iIter = componentMap[i].begin(); iIter != componentMap[i].end(); ++iIter) {
				for (std::vector<unsigned int>::const_iterator kIter = componentMap[k].begin(); kIter != componentMap[k].end(); ++kIter) {
					iPoint = *this->netPoints[*iIter].getPoint();
					kPoint = *this->netPoints[*kIter].getPoint();

					if(iPoint.distance_to(kPoint) < min_dist) {
						min_dist = iPoint.distance_to(kPoint);
						netPointA = *iIter;
						netPointB = *kIter;
					}
				}
			}
			RubberBand* rb = new RubberBand(this->getName(), &this->netPoints[netPointA], &this->netPoints[netPointB], false);
			this->unwiredRubberBands.push_back(rb);
		}
	}

	return &this->unwiredRubberBands;
}

}
