#ifndef slic3r_ElectronicNet_hpp_
#define slic3r_ElectronicNet_hpp_

#include "libslic3r.h"
#include <vector>
#include "Point.hpp"


namespace Slic3r {

struct ElectronicNetPin {
	std::string part;
	std::string pin;
	std::string gate;
};

class ElectronicNet;
class RubberBand;
typedef std::vector<ElectronicNet*> ElectronicNets;
typedef std::vector<RubberBand*> RubberBandPtrs;
typedef std::vector<ElectronicNetPin> Pinlist;

class ElectronicNet
{
    public:
    //ElectronicNet(std::string name);
    ElectronicNet(std::string name);
    ~ElectronicNet();
    std::string getName();
    void addPin(std::string part, std::string pin, std::string gate);
    Pinlist* getPinList();

	private:
    std::string name;
    Pinlist pinlist;

};


class RubberBand
{
	public:
	RubberBand();
	~RubberBand();

	Pointf3 a;
	Pointf3 b;

	private:

};

}

#endif
