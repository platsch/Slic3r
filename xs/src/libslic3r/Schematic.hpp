#ifndef slic3r_Schematic_hpp_
#define slic3r_Schematic_hpp_

#include "libslic3r.h"
#include "ElectronicPart.hpp"
#include "ElectronicNet.hpp"
#include "RubberBand.hpp"
#include "NetPoint.hpp"
#include "Point.hpp"
#include <vector>


namespace Slic3r {

class Schematic
{
    public:
    Schematic();
    ~Schematic();
    void setRootOffset(Pointf3 offset);
	Pointf3 getRootOffset();
	void setFilename(std::string filename);
    void addElectronicPart(ElectronicPart* part);
    ElectronicPart* addElectronicPart(std::string name, std::string library, std::string deviceset, std::string device, std::string package);
    ElectronicPart* getElectronicPart(unsigned int partID);
    void addElectronicNet(ElectronicNet* net);
    ElectronicParts* getPartlist();
    RubberBandPtrs* getRubberBands();
    NetPointPtrs* getNetPoints();
    void splitWire(const RubberBand* rubberband, const Pointf3& p);
    bool removeWire(const unsigned int rubberBandID);
    bool removeNetPoint(const NetPoint* netPoint);


	private:
    bool _checkRubberBandVisibility(const RubberBand* rb, const double z);
    void _updateUnwiredRubberbands();
    void _updateUnwiredRubberbands(ElectronicNet* net);
    void _updateWiredRubberbands();
    void _updateWiredRubberbands(ElectronicNet* net);

    ElectronicNets netlist;
    ElectronicParts partlist;
    std::string filename;
    Pointf3* rootOffset;
    RubberBandPtrs rubberBands;
    NetPointPtrs netPoints;

};

}

#endif
