#ifndef slic3r_ElectronicNet_hpp_
#define slic3r_ElectronicNet_hpp_

#include "libslic3r.h"
#include <vector>
#include <map>
#include <list>
#include "Point.hpp"
#include "NetPoint.hpp"
#include "RubberBand.hpp"

#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/graph_traits.hpp>
#include <boost/graph/connected_components.hpp>
#include <boost/graph/graphviz.hpp>

namespace Slic3r {

// stores data from the schematic
struct ElectronicNetPin {
    std::string part;
    std::string pin;
    std::string gate;
    unsigned int partID;
    unsigned int netPointKey;
};



class ElectronicNet;
typedef std::vector<ElectronicNet*> ElectronicNets;
typedef std::vector<ElectronicNetPin> Pinlist;
typedef boost::adjacency_list <boost::vecS, boost::setS, boost::undirectedS, boost::property<boost::vertex_index_t, unsigned int> > Graph;
typedef boost::graph_traits<Graph>::vertex_descriptor vertex_t;

class ElectronicNet
{
    public:
    ElectronicNet(std::string name);
    ~ElectronicNet();
    std::string getName();
    void addPin(std::string part, std::string pin, std::string gate);
    Pinlist* getPinList();
    ElectronicNetPin* findNetPin(const std::string partName, const std::string pinName);
    ElectronicNetPin* findNetPin(const unsigned int netPointKey);
    unsigned int addNetPoint(const netPointType type, Pointf3 p);
    void updateNetPoint(const unsigned int netPointKey, const netPointType type, const Pointf3 p);
    bool removeNetPoint(unsigned int netPointKey);
    NetPoint* getNetPoint(const unsigned int netPointKey) {return &this->netPoints[netPointKey];};
    bool addWire(const unsigned int netPointAiD, const unsigned int netPointBiD);
    bool removeWire(const unsigned int netPointAiD, const unsigned int netPointBiD);
    //bool addWiredRubberBand(RubberBand* rb);
    bool removeWiredRubberBand(const unsigned int ID);
    const NetPoint* findNearestNetPoint(const Pointf3& p) const;
    bool waypointsConnected(const unsigned int NetPointKeyA, const unsigned int netPointKeyB);
    const unsigned int wayointDegree(const unsigned int waypointID);
    RubberBandPtrs* generateWiredRubberBands();
    RubberBandPtrs* getUnwiredRubberbands();

    private:
    //bool _pointIsConnected(unsigned int netPointKey);

    std::string name;
    Pinlist netPins;
    std::map<unsigned int, NetPoint> netPoints;
    unsigned int currentNetPoint; //pointer to last element
    RubberBandPtrs wiredRubberBands;
    RubberBandPtrs unwiredRubberBands;
    Graph netGraph;
    boost::property_map<Graph, boost::vertex_index_t>::type netGraphIndex; // Index Vertex->netPointKey
    std::map<unsigned int, vertex_t> netPointIndex; // Index netPointKey->Vertex

    friend class Schematic;
};

}

#endif
