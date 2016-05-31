#ifndef slic3r_ElectronicNet_hpp_
#define slic3r_ElectronicNet_hpp_

#include "libslic3r.h"
#include <vector>
#include <map>
#include <list>
#include "Point.hpp"
#include "NetPoint.hpp"
#include "RubberBand.hpp"


namespace Slic3r {

// stores data from the schematic
struct ElectronicNetPin {
	std::string part;
	std::string pin;
	std::string gate;
	unsigned int partID;
};

// stores internal routing information. Endpoints can be points or pins, invalid endpoints must be -1.
struct Wire {
	long pointA;
	long pinA;
	long pointB;
	long pinB;
	// potential additional information e.g. min trace width.
};

class ElectronicNet;
typedef std::vector<ElectronicNet*> ElectronicNets;
typedef std::vector<ElectronicNetPin> Pinlist;

class ElectronicNet
{
    public:
    ElectronicNet(std::string name);
    ~ElectronicNet();
    std::string getName();
    void addPin(std::string part, std::string pin, std::string gate);
    Pinlist* getPinList();
    long findNetPin(const std::string partName, const std::string pinName);
    unsigned int addNetPoint(Pointf3 p);
    bool removeNetPoint(unsigned int netPointID);
    //bool addNetWire(Wire& wire);
    bool addWiredRubberBand(RubberBand* rb);
    bool removeWiredRubberBand(const unsigned int ID);
    unsigned int findNearestNetPoint(const Pointf3& p) const;

	private:
    bool _pointIsConnected(unsigned int netPointID);

    std::string name;
    Pinlist netPins;
    std::map<unsigned int, NetPoint> netPoints;
    unsigned int currentNetPoint; //pointer to last element
    //std::list<Wire> netWires;
    RubberBandPtrs wiredRubberBands;
    RubberBandPtrs unwiredRubberBands;

    friend class Schematic;
};

}

#endif
