#ifndef slic3r_ElectronicWireGenerator_hpp_
#define slic3r_ElectronicWireGenerator_hpp_

#include "libslic3r.h"
#include "Layer.hpp"
#include "Print.hpp"
#include "Polyline.hpp"
#include "Polygon.hpp"
#include "ExPolygonCollection.hpp"

namespace Slic3r {


class ElectronicWireGenerator {
public:
    ElectronicWireGenerator(
        /// Input:
        /// pointer to layer object on which wires will be refined
        Layer*                     layer,
        double                     extrusion_width,
        double                     extrusion_overlap,
        double                     first_extrusion_overlap,
        double                     overlap_min_extrusion_length,
        double                     conductive_wire_channel_width);

    void process();

private:
    void refine_wires();
    void sort_unrouted_wires();
    void channel_from_wire(Polyline &wire);
    void apply_overlap(Polylines input, Polylines *output);
    coord_t offset_width(const LayerRegion* region, int perimeters) const;

    // Inputs:
    Layer* layer;             ///< pointer to layer object
    Polylines unrouted_wires; ///< copy of original path collection,
                              /// will be deleted during process()
    Polylines routed_wires; ///< intermediate state after contour following
    double extrusion_width;
    double extrusion_overlap;
    double first_extrusion_overlap;
    double overlap_min_extrusion_length;
    double conductive_wire_channel_width;
    const ExPolygonCollection* slices;
};

}

#endif
