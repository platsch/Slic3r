#include "NetPoint.hpp"

namespace Slic3r {


NetPoint::NetPoint(const NetPoint &other)
    : key(other.key), type(other.type), netName(other.netName), point(other.point)
{
}

NetPoint::NetPoint(const unsigned int key, const netPointType type, const std::string net, const Pointf3 point)
    : key(key), type(type), netName(net), point(point)
{
}

NetPoint::~NetPoint()
{
}

NetPoint& NetPoint::operator= (const NetPoint &other)
{
    this->key = other.key;
    this->type = other.type;
    this->netName = other.netName;
    this->point = other.point;
    return *this;
}

void NetPoint::setPosition(double x, double y, double z)
{
    this->point = Pointf3(x, y, z);
}

bool NetPoint::setRouteExtensionPoints(const Pointf3 innerPoint, const Pointf3 outerPoint)
{
    bool result = false;
    if(this->type == PART) {
        result = true;
        this->padInnerPoint = innerPoint;
        this->padOuterPoint = outerPoint;
    }

    return result;
}

/* Find point on inner / outer end of the pad to extend
 * the conductive trace for better footprint coverage
 */
Pointf3 NetPoint::getRouteExtension(const Pointf3 fromPoint)
{
    Pointf3 result = this->point;

    if(this->type == PART) {
        if(fromPoint.distance_to(this->padInnerPoint) > fromPoint.distance_to(this->padOuterPoint)) {
            result = this->padInnerPoint;
        }else{
            result = this->padOuterPoint;
        }
    }
    return result;
}

}
