#ifndef slic3r_ElectronicNet_hpp_
#define slic3r_ElectronicNet_hpp_

#include "libslic3r.h"
#include <vector>
#include "Point.hpp"


namespace Slic3r {

class ElectronicNet;
class ElectronicNetPin;

class ElectronicNet
{
    public:
    //ElectronicNet(std::string name);
    ElectronicNet(std::string name);
    ~ElectronicNet();
    void addPin(std::string part, std::string pin, std::string gate);
	private:
    std::string name;
    std::vector<ElectronicNetPin*> pinlist;

};

class ElectronicNetPin
{
	public:
	ElectronicNetPin(std::string part, std::string pin, std::string gate);
	~ElectronicNetPin();
	private:
	std::string part;
	std::string pin;
	std::string gate;
};

}

#endif
