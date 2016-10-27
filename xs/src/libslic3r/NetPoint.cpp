#include "NetPoint.hpp"

namespace Slic3r {


NetPoint::NetPoint(const NetPoint &other)
	: key(other.key), type(other.type), netName(other.netName), point(other.point)
{
}

NetPoint::NetPoint(const unsigned int key, const netPointType type, const std::string net, const Pointf3 point)
{
	this->key = key;
	this->type = type;
	this->netName = net;
	this->point = point;
}

NetPoint::~NetPoint()
{
}

NetPoint& NetPoint::operator= (NetPoint other)
{
	this->key = other.key;
	this->type = other.type;
	this->netName = other.netName;
	this->point = other.point;
    return *this;
}

}
