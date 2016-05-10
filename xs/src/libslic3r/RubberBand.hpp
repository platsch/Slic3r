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
	RubberBand(std::string net, unsigned int partA);
	~RubberBand();
	unsigned int getID() {return this->ID;};
	void addPartB(unsigned int partB);
	const Pointf3* selectNearest(const Pointf3& p);

	Pointf3 a;
	Pointf3 b;

	private:
	std::string netName;
	unsigned int partA;
	unsigned int partB;
	bool hasPartB;
    static unsigned int s_idGenerator;
    unsigned int ID;
};

}

#endif
