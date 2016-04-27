#include "ElectronicNet.hpp"

namespace Slic3r {


ElectronicNet::ElectronicNet(std::string name)
{
	this->name = name;
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
	this->pinlist.push_back(netPin);
}

Pinlist* ElectronicNet::getPinList()
{
	return &this->pinlist;
}

}
