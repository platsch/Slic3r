#ifndef slic3r_Schematic_hpp_
#define slic3r_Schematic_hpp_

#include "libslic3r.h"
#include "ElectronicPart.hpp"
#include "AdditionalPart.hpp"
#include "ElectronicNet.hpp"
#include "RubberBand.hpp"
#include "NetPoint.hpp"
#include "Point.hpp"
#include "Polyline.hpp"
#include "../pugixml/pugixml.hpp"
#include <vector>
#include <list>


namespace Slic3r {

class Schematic
{
    public:
    Schematic();
    Schematic(const Schematic &other);
    ~Schematic();
    Schematic& operator=(const Schematic &other);
    void setFilename(std::string filename);
    std::string getFilename() const {return this->filename;};
    void addElectronicPart(ElectronicPart* part);
    ElectronicPart* addElectronicPart(std::string name, std::string library, std::string deviceset, std::string device, std::string package);
    ElectronicPart* getElectronicPart(unsigned int partID);
    ElectronicPart* getElectronicPart(std::string partName);
    void addElectronicNet(ElectronicNet* net);
    ElectronicParts* getPartlist();
    AdditionalParts* getAdditionalPartlist();
    void addAdditionalPart(std::string thread_size, std::string type);
    RubberBandPtrs* getRubberBands() {return &this->rubberBands;};
    void updateRubberBands();
    NetPointPtrs* getNetPoints();
    const NetPoint* findNearestSplittingPoint(const NetPoint* sourceWayPoint, const Pointf3& p) const;
    void addWire(const NetPoint* netPoint, const Pointf3& p);
    void splitWire(const RubberBand* rubberband, const Pointf3& p);
    bool removeWire(const unsigned int rubberBandID);
    bool removeNetPoint(const NetPoint* netPoint);

    void updatePartNetPoints();
    void updatePartNetPoints(ElectronicPart* part);

    Polylines getChannels(const double z_bottom, const double z_top, coord_t extrusion_overlap, coord_t first_extrusion_overlap, coord_t overlap_min_extrusion_length, coord_t layer_overlap);

    bool write3deFile(std::string filename, std::string filebase);
    bool load3deFile(std::string filename);


    private:
    bool _checkRubberBandVisibility(const RubberBand* rb, const double z);

    ElectronicNets netlist;
    ElectronicParts partlist;
    AdditionalParts additionalPartlist;
    std::string filename;
    RubberBandPtrs rubberBands;
    NetPointPtrs netPoints;

};

}

#endif
