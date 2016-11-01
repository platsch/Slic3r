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
	NetPoint& operator= (NetPoint other);
	NetPoint(const unsigned int key, const netPointType type, const std::string net, const Pointf3 point);
	~NetPoint();
	const unsigned int getKey() const {return this->key;};
	const std::string getNetName() const {return this->netName;};
	Pointf3 getPoint() const {return this->point;};
	const netPointType getType() const {return this->type;};

	private:
	unsigned int key;
	netPointType type;
	std::string netName;
	Pointf3 point;

	friend class ElectronicNet;
};

}

#endif
