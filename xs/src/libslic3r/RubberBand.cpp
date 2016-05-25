#include "RubberBand.hpp"

namespace Slic3r {


RubberBand::RubberBand(const std::string net, Pointf3 a, Pointf3 b):netName(net)
{
	this->ID = this->s_idGenerator++;
	this->a = a;
	this->b = b;
	this->_hasPartA = false;
	this->_hasPartB = false;
	this->_hasNetPointA = false;
	this->_hasNetPointB = false;
	this->wiredFlag = false;
}

RubberBand::~RubberBand()
{
}

// returns true if a is not already defined as a netPoint
bool RubberBand::addPartA(unsigned int partID, unsigned int netPinID)
{
	bool result = false;
	if(!this->_hasNetPointA) {
		this->partAiD = partID;
		this->netPinAiD = netPinID;
		this->_hasPartA = true;
		result = true;
	}
	return result;
}

// returns true if b is not already defined as a netPoint
bool RubberBand::addPartB(unsigned int partID, unsigned int netPinID)
{
	bool result = false;
	if(!this->_hasNetPointB) {
		this->partBiD = partID;
		this->netPinBiD = netPinID;
		this->_hasPartB = true;
		result = true;
	}
	return result;
}

bool RubberBand::addNetPointA(unsigned int netPointAiD)
{
	bool result = false;
	if(!this->_hasPartA) {
		this->netPointAiD = netPointAiD;
		this->_hasNetPointA = true;
	}
	return result;
}

bool RubberBand::addNetPointB(unsigned int netPointBiD)
{
	bool result = false;
	if(!this->_hasPartB) {
		this->netPointBiD = netPointBiD;
		this->_hasNetPointB = true;
	}
	return result;
}

bool RubberBand::connectsNetPin(const unsigned int netPinID) const
{
	bool result = false;
	if(this->hasPartA() && (this->netPinAiD == netPinID)) {
		result = true;
	}
	if(this->hasPartB() && (this->netPinBiD == netPinID)) {
		result = true;
	}
	return result;
}

const Pointf3* RubberBand::selectNearest(const Pointf3& p)
{
	const Pointf3* result = &(this->a);
	if(this->_hasPartB) {
		if(this->a.distance_to(p) > this->b.distance_to(p)) {
			result = &(this->b);
		}
	}

	return result;
}

// Initialization of static ID generator variable
unsigned int RubberBand::s_idGenerator = 1;

}
