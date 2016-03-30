#include "ElectronicNet.hpp"

namespace Slic3r {


ElectronicNet::ElectronicNet(std::string name)
{
	this->name = name;
}

ElectronicNet::~ElectronicNet()
{
}

void
ElectronicNet::addPin(std::string part, std::string pin, std::string gate)
{
	this->pinlist.push_back(new ElectronicNetPin(part, pin, gate));
}

}
