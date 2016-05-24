#ifndef slic3r_RubberBand_hpp_
#define slic3r_RubberBand_hpp_

#include "libslic3r.h"
#include "ElectronicPart.hpp"
#include <vector>
#include <map>
#include <list>
#include "Point.hpp"


namespace Slic3r {

class RubberBand;
typedef std::vector<RubberBand*> RubberBandPtrs;

/* A "real" rubberband connects two unwired parts or one part and a point of partially
 * wired net. Rubberbanding might also be used to visualize wired net connections over
 * multiple layers or non-straight geometry.
 */

class RubberBand
{
	public:
	RubberBand(const std::string net, const Pointf3& a, const Pointf3& b);
	~RubberBand();
	const unsigned int getID() const {return this->ID;};
	const std::string getNetName() const {return this->netName;};
	bool addPartA(unsigned int partID, unsigned int netPinID);
	bool addPartB(unsigned int partID, unsigned int netPinID);
	bool addNetPointA(unsigned int netPointAiD);
	bool addNetPointB(unsigned int netPointBiD);
	const bool hasPartA() const {return this->_hasPartA;};
	const bool hasPartB() const {return this->_hasPartB;};
	const bool hasNetPointA() const {return this->_hasNetPointA;};
	const bool hasNetPointB() const {return this->_hasNetPointB;};
	const unsigned int getPartAiD() const {return this->partAiD;};
	const unsigned int getPartBiD() const {return this->partBiD;};
	const unsigned int getNetPinAiD() const {return this->netPinAiD;};
	const unsigned int getNetPinBiD() const {return this->netPinBiD;};
	const unsigned int getNetPointAiD() const {return this->netPointAiD;};
	const unsigned int getNetPointBiD() const {return this->netPointBiD;};
	void setWired(bool wired) {this->wiredFlag = wired;};
	const bool isWired() const {return this->wiredFlag;};
	bool connectsNetPin(const unsigned int netPinID) const;
	const Pointf3* selectNearest(const Pointf3& p);

	const Pointf3 a;
	const Pointf3 b;

	private:
	const std::string netName;
	unsigned int partAiD;
	unsigned int partBiD;
	unsigned int netPinAiD;
	unsigned int netPinBiD;
	unsigned int netPointAiD;
	unsigned int netPointBiD;
	bool _hasPartA;
	bool _hasPartB;
	bool _hasNetPointA;
	bool _hasNetPointB;
	bool wiredFlag;
    static unsigned int s_idGenerator;
    unsigned int ID;
};

}

#endif
