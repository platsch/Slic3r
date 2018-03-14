#include "ElectronicWireGenerator.hpp"
#include "ClipperUtils.hpp"
#include  "SVG.hpp"

namespace Slic3r {

ElectronicWireGenerator::ElectronicWireGenerator(Layer* layer, double extrusion_width,
        double extrusion_overlap, double first_extrusion_overlap, double overlap_min_extrusion_length, double conductive_wire_channel_width) :
        layer(layer),
        extrusion_width(extrusion_width),
        extrusion_overlap(extrusion_overlap),
        first_extrusion_overlap(first_extrusion_overlap),
        overlap_min_extrusion_length(overlap_min_extrusion_length),
        conductive_wire_channel_width(conductive_wire_channel_width),
        slices(&(layer->slices)),
        unrouted_wires(layer->unrouted_wires){}

void
ElectronicWireGenerator::process()
{
    // contour following should happen at this point...
    this->refine_wires();

    // cut wires and apply overlaps
    this->apply_overlap(this->routed_wires, &(this->layer->wires));

    SVG svg("refine_wires_result.svg");
    svg.draw(*(this->slices), "black");
    svg.draw(this->layer->wires, "red", scale_(this->extrusion_width)/2);
    svg.arrows = false;
    svg.Close();
}

void ElectronicWireGenerator::refine_wires()
{
    // clip polyline against polygon. Check result: identical? split into multiple parts?

    SVG svg("refine_wires.svg");
    svg.draw(*(this->slices), "black");
    bool debug_flag = true;
    //svg.draw(this->unrouted_wires, "red");
    int index = 0;

    // number of applicable perimeters
    int max_perimeters = 9999;
    for(const auto &region : this->layer->regions) {
      max_perimeters = std::min(max_perimeters, region->region()->config.perimeters.value);
    }
    // ensure at least 1 perimeter for polygon offsetting
    max_perimeters = std::max(1, max_perimeters);

    this->sort_unrouted_wires();

    for(const auto &unrouted_wire : this->unrouted_wires) {

        std::ostringstream ss;
        ss << "wire_debug_";
        ss << index;
        ss << ".svg";
        std::string filename = ss.str();
        std::cout << "filename: " << filename << std::endl;
        SVG svg(filename.c_str());
        debug_flag = true;
        index++;
        svg.draw(*(this->slices), "black");

        Polyline routed_wire;
        routed_wire.append(unrouted_wire.first_point());// add first point
        for(auto &line : unrouted_wire.lines()) {

            // generate set of inflated expolygons. Inflate by perimeter width per region and channel width.
            ExPolygonCollection inflated_slices;
            std::vector<ExPolygonCollection> inflated_slices2;

            for(int i=0; i < max_perimeters; i++) {
                // initialize vector element
                inflated_slices2.push_back(ExPolygonCollection());

                for(const auto &region : this->layer->regions) {

                    /*const coord_t perimeter_spacing     = region->flow(frPerimeter).scaled_spacing();
                    const coord_t ext_perimeter_width   = region->flow(frExternalPerimeter).scaled_width();
                    const coord_t ext_perimeter_spacing = region->flow(frExternalPerimeter).scaled_spacing();
                    // compute the total thickness of perimeters
                    const coord_t perimeters_thickness = ext_perimeter_width
                        + (region->region()->config.perimeters-1) * perimeter_spacing
                        + scale_(this->extrusion_width/2 + this->conductive_wire_channel_width/2);
                    */
                    //const coord_t perimeters_thickness = this->offset_width(region, region->region()->config.perimeters);
                    const coord_t perimeters_thickness = this->offset_width(region, i+1);

    /*                std::cout << std::endl;
                    std::cout << "perimeter_spacing: " << perimeter_spacing << std::endl;
                    std::cout << "ext_perimeter_width: " << ext_perimeter_width << std::endl;
                    std::cout << "ext_perimeter_spacing: " << ext_perimeter_spacing << std::endl;
                    std::cout << "perimeters_thickness: " << unscale(perimeters_thickness) << std::endl;
                    std::cout << "external perimeter: " << ext_perimeter_width << std::endl;
                    std::cout << "internal perimeters: " << (region->region()->config.perimeters-1) * perimeter_spacing << std::endl;
                    std::cout << "number of internal perimeters: " << region->region()->config.perimeters << std::endl;
                    std::cout << "half channel: " << scale_(this->extrusion_width/2 + this->conductive_wire_channel_width/2) << std::endl;
                    std::cout << "extrusion width : " << scale_(this->extrusion_width/2) << " + channel width: " << scale_(this->conductive_wire_channel_width/2) << std::endl;
                    std::cout << std::endl;
    */

                    //inflated_slices.append(offset_ex((Polygons)region->slices, -perimeters_thickness));
                    inflated_slices2[i].append(offset_ex((Polygons)region->slices, -perimeters_thickness));
                }
            }
            if(debug_flag) {
                int c = 50;
                for(int i=0; i < max_perimeters; i++) {
                    std::ostringstream ss;
                    ss << "rgb(" << 100 << "," << c << "," << c << ")";
                    std::string color = ss.str();
                    svg.draw(inflated_slices2[i], color);
                    c += 50;
                }
                debug_flag = false;
            }

            bool outside_expolygon = false;
            int current_perimeters = 1;
            while(true) {
                Point intersection;
                bool ccw;
                const Polygon* p;
                const ExPolygon* ep;
                // find intersections with all expolygons
                while(inflated_slices2[current_perimeters].first_intersection(line, &intersection, &ccw, &p, &ep)) {
                    std::cout << "in loop" << std::endl;
                    if(ccw) { // leaving expolygon
                        outside_expolygon = true;
                        svg.draw(Line(line.a, intersection), "red", scale_(this->extrusion_width));
                        Line l(line.a, intersection);
                        l.extend_end(scale_(EPSILON));
                        intersection = l.b;
                        routed_wire.append(intersection);
                        current_perimeters = 1;
                    }else{ // entering expolygon
                        // follow contour
                        if(routed_wire.is_valid() && outside_expolygon) { // at least 2 points
                            outside_expolygon = false;
                            Polyline result_pl;
                            //while(current_perimeters <= max_perimeters) {
                                // convert to polyline
                                Polyline pl = *p;
                                // split at entry point
                                Polyline pl_tmp, pl1, pl2;
                                pl.split_at(routed_wire.last_point(), &pl1, &pl_tmp);
                                pl_tmp.append(pl1); // append first half (until entry split) to second half (rest)
                                pl_tmp.split_at(intersection, &pl1, &pl2);
                                // compare lenghts, add shorter polyline
                                if(pl1.length() < pl2.length()) {
                                    //routed_wire.append(pl1);
                                    result_pl = pl1;
                                    svg.draw(pl1, "green", scale_(this->extrusion_width));
                                    svg.draw(pl2, "orange", scale_(this->extrusion_width)/2);
                                }else{
                                    pl2.reverse();
                                    //routed_wire.append(pl2);
                                    result_pl = pl2;
                                    svg.draw(pl2, "green", scale_(this->extrusion_width));
                                    svg.draw(pl1, "orange", scale_(this->extrusion_width)/2);
                                }
                             //   current_perimeters++;
                            //}
                            routed_wire.append(result_pl);
                        }
                        std::cout << "routing done" << std::endl;
                        Line l(line.a, intersection);
                        l.extend_end(scale_(EPSILON));
                        intersection = l.b;
                        svg.draw(Line(routed_wire.last_point(), intersection), "red", scale_(this->extrusion_width));
                        routed_wire.append(intersection);
                    }
                    line.a = intersection;
                }
                // no further intersections found, rest must be inside or outside of slices. Append linear line
                svg.draw(line, "red", scale_(this->extrusion_width));
                routed_wire.append(line.b);
                std::cout << "append rest" << std::endl;
                break;
            }
        }
        svg.draw(routed_wire, "yellow", scale_(this->extrusion_width));
        routed_wire.remove_loops();
        this->channel_from_wire(routed_wire);
        this->routed_wires.push_back(routed_wire);
        svg.draw(routed_wire, "white", scale_(this->extrusion_width)/2);

        svg.arrows = false;
        svg.Close();

        // any perimeter contact for this wire?
        //Polylines diff_pl = diff_pl(wire.Polylines(), const Slic3r::Polygons &clip, bool safety_offset_ = false)
    }
}

void ElectronicWireGenerator::sort_unrouted_wires()
{
    // number of applicable perimeters
    int perimeters = 9999;
    for(const auto &region : this->layer->regions) {
      perimeters = std::min(perimeters, region->region()->config.perimeters.value);
    }
    coord_t wire_offset = this->offset_width(this->layer->regions.front(), perimeters);

    // generate channel expolygonCollection
    ExPolygonCollection inflated_wires;
    for(const auto &unrouted_wire : this->unrouted_wires) {
        // double offsetting for channels. 1 step generates a polygon from the polyline,
        // 2. step extends the polygon to avoid cropped angles.
        inflated_wires.append(offset_ex(offset(unrouted_wire, scale_(0.01)), wire_offset - scale_(0.01)));
    }

    // count intersections with channels created by other wires
    std::vector<unsigned int> intersections;
    for(const auto &unrouted_wire : this->unrouted_wires) {
        intersections.push_back(0);

        for(auto &line : unrouted_wire.lines()) {
            Point intersection;
            bool ccw;
            const Polygon* p;
            const ExPolygon* ep;
            while(inflated_wires.first_intersection(line, &intersection, &ccw, &p, &ep)) {
                intersections.back()++;
                Line l(line.a, intersection);
                l.extend_end(scale_(EPSILON));
                line.a = l.b;
            }
        }
    }

    // perform actual sort
    Polylines local_copy = std::move(this->unrouted_wires);
    this->unrouted_wires.clear();
    while(intersections.size() > 0) {
        int min_pos = 0;
        double min_len = 0;
        unsigned int min_intersections = 999999;
        for(int i = 0; i < intersections.size(); i++) {
            //second stage of sorting: by line length
            if(intersections[i] == min_intersections) {
                if(local_copy[i].length() < min_len) {
                    min_intersections = intersections[i];
                    min_pos = i;
                    min_len = local_copy[i].length();
                }
            }
            // first stage of sorting: by intersections
            if(intersections[i] < min_intersections) {
                min_intersections = intersections[i];
                min_pos = i;
                min_len = local_copy[i].length();
            }
        }
        this->unrouted_wires.push_back(local_copy[min_pos]);
        local_copy.erase(local_copy.begin()+min_pos);
        intersections.erase(intersections.begin()+min_pos);
    }
}

void ElectronicWireGenerator::channel_from_wire(Polyline &wire)
{
    Polygons bed_polygons;
    Polygons channel_polygons;

    // double offsetting for channels. 1 step generates a polygon from the polyline,
    // 2. step extends the polygon to avoid cropped angles.
    bed_polygons = offset(wire, scale_(0.01));
    channel_polygons = offset(bed_polygons, scale_(this->extrusion_width/2 + this->conductive_wire_channel_width/2 - 0.01));

    // remove beds and channels from layer
    for(const auto &region : this->layer->regions) {
        region->modify_slices(channel_polygons, false);
    }

    // if lower_layer is defined, use channel to create a "bed" by offsetting only a small amount
    // generate a bed by offsetting a small amount to trigger perimeter generation
    if (this->layer->lower_layer != nullptr) {
        FOREACH_LAYERREGION(this->layer->lower_layer, layerm) {
            (*layerm)->modify_slices(bed_polygons, false);
        }
        this->layer->lower_layer->setDirty(true);
    }
    this->layer->setDirty(true);
}

// clip wires to extrude from center to smd-pad and add overlaps
// at extrusion start points
void ElectronicWireGenerator::apply_overlap(Polylines input, Polylines *output)
{
    if(input.size() > 0) {
        // find longest path for this layer and apply higher overlapping
        Polylines::iterator max_len_pl;
        double max_len = 0;
        for (Polylines::iterator pl = input.begin(); pl != input.end(); ++pl) {
            if(pl->length() > max_len) {
                max_len = pl->length();
                max_len_pl = pl;
            }
        }
        // move longest line to front
        Polyline longest_line = (*max_len_pl);
        input.erase(max_len_pl);
        input.insert(input.begin(), longest_line);

        // split paths to equal length parts with small overlap to have extrusion ending at endpoint
        for(auto &pl : input) {
            double clip_length;
            if(&pl == &input.front()) {
                clip_length = pl.length()/2 - this->first_extrusion_overlap; // first extrusion at this layer
            }else{
                clip_length = pl.length()/2 - this->extrusion_overlap; // normal extrusion overlap
            }

            // clip start and end of each trace by extrusion_width/2 to achieve correct line endpoints
            pl.clip_start(scale_(this->extrusion_width/2));
            pl.clip_end(scale_(this->extrusion_width/2));

            // clip line if long enough and push to wires collection
            if(((pl.length()/2 - clip_length) > EPSILON) && (pl.length() > this->overlap_min_extrusion_length)) {
                Polyline pl2 = pl;

                 //first half
                pl.clip_end(clip_length);
                pl.reverse();
                pl.remove_duplicate_points();
                if(pl.length() > scale_(0.05)) {
                    output->push_back(pl);
                }

                //second half
                pl2.clip_start(clip_length);
                pl2.remove_duplicate_points();
                if(pl2.length() > scale_(0.05)) {
                    output->push_back(pl2);
                }
            }else{ // just push it to the wires collection otherwise
                output->push_back(pl);
            }
        }
    }
}

coord_t ElectronicWireGenerator::offset_width(const LayerRegion* region, int perimeters) const
{
    perimeters = std::max(perimeters, 1);
    const coord_t perimeter_spacing     = region->flow(frPerimeter).scaled_spacing();
    const coord_t ext_perimeter_width   = region->flow(frExternalPerimeter).scaled_width();
    // compute the total thickness of perimeters
    const coord_t result = ext_perimeter_width
        + (perimeters-1) * perimeter_spacing
        + scale_(this->extrusion_width/2 + this->conductive_wire_channel_width/2);
    return result;
}

}
