#include "ElectronicNet.hpp"

namespace Slic3r {


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
	this->netPins.push_back(netPin);
}

Pinlist* ElectronicNet::getPinList()
{
	return &this->netPins;
}

long ElectronicNet::findNetPin(const std::string partName, const std::string pinName)
{
	long result = -1;
	for (int i = 0; i < this->netPins.size(); i++) {
		if(this->netPins[i].part == partName && this->netPins[i].pin == pinName) {
			result = i;
			break;
		}
	}
	return result;
}

unsigned int ElectronicNet::addNetPoint(Pointf3 p)
{
	this->currentNetPoint++;
	this->netPoints[this->currentNetPoint] = NetPoint(this->name, p);
	return this->currentNetPoint;
}

void ElectronicNet::removeNetPoint(unsigned int netPointID)
{
	this->netPoints.erase(netPointID);
}

/*
bool ElectronicNet::addNetWire(Wire& wire)
{
	bool result = false;

	// check if given points exist and exactly one A and one B are defined
	if((wire.pointA == -1 && wire.pinA >= 0) || (wire.pinA == -1 && wire.pointA >= 0)) {
		if((wire.pointB == -1 && wire.pinB >= 0) || (wire.pinB == -1 && wire.pointB >= 0)) {
			this->netWires.push_back(wire);
			result = true;
		}
	}
	return result;
}
*/
bool ElectronicNet::addWiredRubberBand(RubberBand* rb)
{
	bool result = false;

		// check if given points exist and exactly one A and one B are defined
	if(rb->hasPartA() || rb->hasNetPointA()) {
		if(rb->hasPartB() || rb->hasNetPointB()) {
			rb->setWired(true);
			this->wiredRubberBands.push_back(rb);
			result = true;
		}else{
			std::cout << "no part / point B" << std::endl;
		}
	}else{
		std::cout << "no part / point A" << std::endl;
	}

	return result;
}

// Removes rubberBand with given ID from the vector, returns true if a matching RB exists, false otherwise
bool ElectronicNet::removeWiredRubberBand(const unsigned int ID)
{
	bool result = false;
	for(int i = 0; i < this->wiredRubberBands.size(); i++) {
		if(this->wiredRubberBands[i]->getID() == ID) {
			this->wiredRubberBands.erase(this->wiredRubberBands.begin() + i);
			result = true;
			break;
		}
	}
	return result;
}

unsigned int ElectronicNet::findNearestNetPoint(const Pointf3& p) const
{
	std::cout << "called findNearestNetPoint" << std::endl;
	unsigned int result = 0;
	double min_dist = 999999999999999.0;
	for (std::map<unsigned int, NetPoint>::const_iterator netPoint = this->netPoints.begin(); netPoint != this->netPoints.end(); ++netPoint) {
	    std::cout << netPoint->first << ", " << netPoint->second.getNetName() << '\n';
	}
}

}
