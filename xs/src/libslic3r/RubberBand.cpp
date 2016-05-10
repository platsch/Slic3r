#include "RubberBand.hpp"

namespace Slic3r {


RubberBand::RubberBand(std::string net, unsigned int partA)
{
	this->ID = this->s_idGenerator++;
	this->netName = net;
	this->partA = partA;
	this->hasPartB = false;
}

RubberBand::~RubberBand()
{
}

void RubberBand::addPartB(unsigned int partB)
{
	this->partB = partB;
	this->hasPartB = true;
}

const Pointf3* RubberBand::selectNearest(const Pointf3& p)
{
	const Pointf3* result = &(this->a);
	if(this->hasPartB) {
		if(this->a.distance_to(p) > this->b.distance_to(p)) {
			result = &(this->b);
		}
	}

	return result;
}

// Initialization of static ID generator variable
unsigned int RubberBand::s_idGenerator = 1;

}
