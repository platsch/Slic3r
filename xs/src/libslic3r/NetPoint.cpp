#include "NetPoint.hpp"

namespace Slic3r {


NetPoint::NetPoint(const NetPoint &other)
	: ID(other.ID), type(other.type), netName(other.netName), point(other.point)
{
}

NetPoint::NetPoint(const unsigned int ID, const netPointType type, const std::string net, const Pointf3 point)
{
	this->ID = ID;
	this->type = type;
	this->netName = net;
	this->point = point;
}

NetPoint::~NetPoint()
{
}

NetPoint& NetPoint::operator= (NetPoint other)
{
	this->ID = other.ID;
	this->type = other.type;
	this->netName = other.netName;
	this->point = other.point;
    return *this;
}

}
