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
	NetPoint(const unsigned int id, const netPointType type, const std::string net, const Pointf3 point);
	~NetPoint();
	const unsigned int getID() const {return this->ID;};
	const std::string getNetName() const {return this->netName;};
	const Pointf3* getPoint() const {return &this->point;};

	private:
	unsigned int ID;
	netPointType type;
	std::string netName;
	Pointf3 point;

	friend class ElectronicNet;
};

}

#endif
