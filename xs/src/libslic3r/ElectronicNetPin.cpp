#include "ElectronicNet.hpp"

namespace Slic3r {

ElectronicNetPin::ElectronicNetPin(std::string part, std::string pin, std::string gate)
{
	this->part = part;
	this->pin = pin;
	this->gate = gate;
}
ElectronicNetPin::~ElectronicNetPin()
{
}

}
