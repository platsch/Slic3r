#include "RubberBand.hpp"

namespace Slic3r {


RubberBand::RubberBand(const std::string net, NetPoint* pointA, NetPoint* pointB, int netPointADegree, int netPointBDegree, bool isWired):
    netName(net),
    wiredFlag(isWired)
{
    this->ID = this->s_idGenerator++;
    this->netPointA = pointA;
    this->netPointB = pointB;
    this->netPointAiD = pointA->getKey();
    this->netPointBiD = pointB->getKey();
    this->a = (pointA->getPoint());
    this->b = (pointB->getPoint());
    this->_pinAExtension = this->a;
    this->_pinBExtension = this->b;
    this->netPointASelected = false;
    this->netPointBSelected = false;

    if(this->netPointA->getType() == PART && netPointADegree == 1) {
        this->_pinAExtension = this->netPointA->getRouteExtension(this->b);
    }
    if(this->netPointB->getType() == PART && netPointBDegree == 1) {
        this->_pinBExtension = this->netPointB->getRouteExtension(this->a);
    }
}

RubberBand::~RubberBand()
{
}

Line3 RubberBand::asScaledLine() const
{
    Line3 result(Point3(scale_(this->a.x), scale_(this->a.y), scale_(this->a.z)),
            Point3(scale_(this->b.x), scale_(this->b.y), scale_(this->b.z)));
    return result;
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
    this->netPointASelected = true;
    if(this->a.distance_to(p) > this->b.distance_to(p)) {
        this->netPointASelected = false;
        this->netPointBSelected = true;
        result = &(this->b);
    }

    return result;
}


/* Computes the segment of this rubberband crossing the given layer interval.
 * Endpoints are extended by extension_length if they lie outside of the layer
 * to ensure inter-layer connectivity.
 * Endpoints connecting an smd-pin are extended to the opposite of the pin if extendPinX is true.
 * extension_length and result in scaled coordinates.
 * Resulting segments preserve order from a to b.
 */
bool RubberBand::getLayerSegments(const double z_bottom, const double z_top, const coord_t layer_overlap, Lines* segments, bool* overlap_a, bool* overlap_b) const
{
    bool result = false;

    // does this rubberband intersect the given layer?
    if((this->a.z >= z_bottom && this->b.z < z_top) || (this->a.z < z_top && this->b.z >= z_bottom)) {
        result = true;
        bool extend_a = true;
        bool extend_b = true;
        Line segment;

        // point a
        if(this->a.z < z_bottom) {
            Pointf3 i = this->intersect_plane(z_bottom);
            segment.a.x = scale_(i.x);
            segment.a.y = scale_(i.y);
        }else if(this->a.z > z_top) {
            Pointf3 i = this->intersect_plane(z_top);
            segment.a.x = scale_(i.x);
            segment.a.y = scale_(i.y);
        }else{
            segment.a.x = scale_(this->a.x);
            segment.a.y = scale_(this->a.y);

            // if this is a pad, extend to pad perimeter
            if(!this->_pinAExtension.coincides_with(this->a)) {
                Line padExtension(Point(scale_(this->_pinAExtension.x), scale_(this->_pinAExtension.y)), segment.a);
                segments->push_back(padExtension);
            }

            extend_a = false;
        }

        // point b
        if(this->b.z < z_bottom) {
            Pointf3 i = this->intersect_plane(z_bottom);
            segment.b.x = scale_(i.x);
            segment.b.y = scale_(i.y);
        }else if(this->b.z > z_top) {
            Pointf3 i = this->intersect_plane(z_top);
            segment.b.x = scale_(i.x);
            segment.b.y = scale_(i.y);
        }else{
            segment.b.x = scale_(this->b.x);
            segment.b.y = scale_(this->b.y);

            extend_b = false;
        }

        if(extend_a && layer_overlap > 0) {
            //segment.extend_start(layer_overlap);
            Line l(segment);
            l.extend_start(layer_overlap/2);
            l.b = segment.a;
            // move 2. half of overlap to l if possible to ensure proper overlap for routing
            if(segment.length() > layer_overlap/2) {
                segment.extend_start(-layer_overlap/2);
                l.b = segment.a;
            }
            segments->push_back(l);
        }

        if(extend_b && layer_overlap > 0) {
            //segment.extend_end(layer_overlap);
            Line l(segment);
            l.extend_end(layer_overlap/2);
            l.a = segment.b;
            if(segment.length() > layer_overlap/2) {
                segment.extend_end(-layer_overlap/2);
                l.a = segment.b;
            }
            // preserve order of lines! a-extension is pushed first, then segment
            segments->push_back(segment);
            segments->push_back(l);
        }else{
            // preserve order of lines! a-extension is pushed first, then segment
            segments->push_back(segment);
        }

        // and then b extension
        // if this is a pad, extend to pad perimeter
        if(!extend_b && !this->_pinBExtension.coincides_with(this->b)) {
            Line padExtension(segment.b, Point(scale_(this->_pinBExtension.x), scale_(this->_pinBExtension.y)));
            segments->push_back(padExtension);
        }

        // if only one point touches the layer, length will be 0
        if(segment.length() < scale_(0.05)) {
            result = false;
        }

        // set overlap flags
        *overlap_a = extend_a;
        *overlap_b = extend_b;
    }

    return result;
}

/* Computes the segment of the full rubberband.
 * Endpoints connecting an smd-pin are extended to the opposite of the pin if extendPinX is true.
 * extension_length and result in scaled coordinates.
 */
Line3s RubberBand::getExtendedSegmets() const
{
    Line3s result;

    // if this is a pad, extend to pad perimeter
    if(!this->_pinAExtension.coincides_with(this->a)) {
        Line3 padExtension(this->_pinAExtension.scaled_point3(), this->a.scaled_point3());
        result.push_back(padExtension);
    }
    // actual rubberband line
    result.push_back(Line3(this->a.scaled_point3(), this->b.scaled_point3()));

    // extend pad b?
    if(!this->_pinBExtension.coincides_with(this->b)) {
        Line3 padExtension(this->b.scaled_point3(), this->_pinBExtension.scaled_point3());
        result.push_back(padExtension);
    }

    return result;
}

// Initialization of static ID generator variable
unsigned int RubberBand::s_idGenerator = 1;

}
