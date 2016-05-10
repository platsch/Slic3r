#ifndef slic3r_ElectronicNet_hpp_
#define slic3r_ElectronicNet_hpp_

#include "libslic3r.h"
#include <vector>
#include <map>
#include <list>
#include "Point.hpp"


namespace Slic3r {

// stores data from the schematic
struct ElectronicNetPin {
	std::string part;
	std::string pin;
	std::string gate;
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

	private:
    std::string name;
    Pinlist pinlist;
    std::map<int, Pointf3> netPoints;
    unsigned int currentNetPoint; //pointer to last element
    std::list<Wire> netWires;
};

}

#endif
