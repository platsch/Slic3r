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
	this->netPoints[this->currentNetPoint] = NetPoint(this->currentNetPoint, this->name, p);
	return this->currentNetPoint;
}

bool ElectronicNet::removeNetPoint(unsigned int netPointID)
{
	bool result = false;
	if(this->netPoints.erase(netPointID) > 0) {
		result = true;
	}

	// remove all wired rubberbands connected to this netPoint
	for (RubberBandPtrs::const_reverse_iterator rubberband = this->wiredRubberBands.rbegin(); rubberband != this->wiredRubberBands.rend(); ++rubberband) {
		bool remove = false;
		if((*rubberband)->hasNetPointA()) {
			if((*rubberband)->getNetPointAiD() == netPointID) {
				remove = true;
			}
		}
		if((*rubberband)->hasNetPointB()) {
			if((*rubberband)->getNetPointBiD() == netPointID) {
				remove = true;
			}
		}
		if(remove) {
			this->removeWiredRubberBand((*rubberband)->getID());
		}
	}

	return result;
}

bool ElectronicNet::addWiredRubberBand(RubberBand* rb)
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

// Removes rubberBand with given ID from the vector, returns true if a matching RB exists, false otherwise
bool ElectronicNet::removeWiredRubberBand(const unsigned int ID)
{
	bool result = false;
	for(int i = 0; i < this->wiredRubberBands.size(); i++) {
		if(this->wiredRubberBands[i]->getID() == ID) {
			unsigned int netPointA = 0, netPointB = 0;
			if(this->wiredRubberBands[i]->hasNetPointA()) netPointA = this->wiredRubberBands[i]->getNetPointAiD();
			if(this->wiredRubberBands[i]->hasNetPointB()) netPointB = this->wiredRubberBands[i]->getNetPointBiD();

			this->wiredRubberBands.erase(this->wiredRubberBands.begin() + i);

			// check for abandoned netPoints
			if(netPointA > 0) {
				if(!_pointIsConnected(netPointA)) {
					this->netPoints.erase(netPointA);
				}
			}
			if(netPointB > 0) {
				if(!_pointIsConnected(netPointB)) {
					this->netPoints.erase(netPointB);
				}
			}

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

bool ElectronicNet::_pointIsConnected(unsigned int netPointID)
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
}

}
