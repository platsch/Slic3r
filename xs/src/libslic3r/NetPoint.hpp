#ifndef slic3r_NetPoint_hpp_
#define slic3r_NetPoint_hpp_

#include "libslic3r.h"
#include "Point.hpp"

namespace Slic3r {

class NetPoint;
typedef std::vector<NetPoint*> NetPointPtrs;

enum netPointType {WAYPOINT, PART};

class NetPoint
{
    public:
    NetPoint() {};
    NetPoint(const NetPoint &other);
    NetPoint& operator= (const NetPoint &other);
    NetPoint(const unsigned int key, const netPointType type, const std::string net, const Pointf3 point);
    ~NetPoint();
    const unsigned int getKey() const {return this->key;};
    const std::string getNetName() const {return this->netName;};
    Pointf3 getPoint() const {return this->point;};
    void setPosition(double x, double y, double z);
    const netPointType getType() const {return this->type;};
    bool setRouteExtensionPoints(const Pointf3 innerPoint, const Pointf3 outerPoint);
    Pointf3 getRouteExtension(const Pointf3 fromPoint);

    private:
    unsigned int key;
    netPointType type;
    std::string netName;
    Pointf3 point;
    Pointf3 padInnerPoint;
    Pointf3 padOuterPoint;

    friend class ElectronicNet;
};

}

#endif
