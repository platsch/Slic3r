#include "RubberBand.hpp"

namespace Slic3r {


RubberBand::RubberBand(const std::string net, NetPoint* pointA, NetPoint* pointB, bool isWired):
	netName(net),
	wiredFlag(isWired)
{
	this->ID = this->s_idGenerator++;
	this->netPointA = pointA;
	this->netPointB = pointB;
	this->netPointAiD = pointA->getKey();
	this->netPointBiD = pointB->getKey();
	this->a = (*pointA->getPoint());
	this->b = (*pointB->getPoint());
}

RubberBand::~RubberBand()
{
}

/*// returns true if a is not already defined as a netPoint
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
*/
bool RubberBand::connectsNetPin(const unsigned int netPinID) const
{
	/*bool result = false;
	if(this->hasPartA() && (this->netPinAiD == netPinID)) {
		result = true;
	}
	if(this->hasPartB() && (this->netPinBiD == netPinID)) {
		result = true;
	}
	return result;*/
	return false;
}

const Pointf3* RubberBand::selectNearest(const Pointf3& p)
{
	const Pointf3* result = &(this->a);
	if(this->a.distance_to(p) > this->b.distance_to(p)) {
		result = &(this->b);
	}

	return result;
}


/* Computes the segment of this rubberband crossing the given layer interval.
 * Endpoints are extended by extension_length if they lie outside of the layer
 * to ensure inter-layer connectivity. extension_length and result in scaled coordinates.
 */
bool RubberBand::getLayerSegment(const double z_bottom, const double z_top, coord_t layer_overlap, Line* segment)
{
	bool result = false;

	// does this rubberband intersect the given layer?
	if((this->a.z >= z_bottom && this->b.z < z_top) || (this->a.z < z_top && this->b.z >= z_bottom)) {
		result = true;
		bool extend_a = true;

		// point a
		if(this->a.z < z_bottom) {
			Pointf3 i = this->intersect_plane(z_bottom);
			segment->a.x = scale_(i.x);
			segment->a.y = scale_(i.y);
		}else if(this->a.z > z_top) {
			Pointf3 i = this->intersect_plane(z_top);
			segment->a.x = scale_(i.x);
			segment->a.y = scale_(i.y);
		}else{
			segment->a.x = scale_(this->a.x);
			segment->a.y = scale_(this->a.y);
			extend_a = false;
		}

		// point b
		if(this->b.z < z_bottom) {
			Pointf3 i = this->intersect_plane(z_bottom);
			segment->b.x = scale_(i.x);
			segment->b.y = scale_(i.y);
			segment->extend_end(layer_overlap);
		}else if(this->b.z > z_top) {
			Pointf3 i = this->intersect_plane(z_top);
			segment->b.x = scale_(i.x);
			segment->b.y = scale_(i.y);
			segment->extend_end(layer_overlap/2);
		}else{
			segment->b.x = scale_(this->b.x);
			segment->b.y = scale_(this->b.y);
		}

		if(extend_a) {
			segment->extend_start(layer_overlap/2);
		}

		// if only one point touches the layer length will be 0
		if(segment->length() < scale_(0.05)) {
			result = false;
		}
	}

	return result;
}

// Initialization of static ID generator variable
unsigned int RubberBand::s_idGenerator = 1;

}
