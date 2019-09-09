#include "ElectronicWireGenerator.hpp"
#include "ClipperUtils.hpp"
#include  "SVG.hpp"

namespace Slic3r {

ElectronicWireGenerator::ElectronicWireGenerator(Layer* layer, ElectronicWireGenerator* previous_ewg, double extrusion_width,
        double extrusion_overlap, double first_extrusion_overlap, double overlap_min_extrusion_length, double conductive_wire_bed_width, double conductive_wire_channel_width, const double grid_step_size) :
        layer(layer),
        previous_ewg(previous_ewg),
        extrusion_width(extrusion_width),
        extrusion_overlap(extrusion_overlap),
        first_extrusion_overlap(first_extrusion_overlap),
        overlap_min_extrusion_length(overlap_min_extrusion_length),
        conductive_wire_bed_width(conductive_wire_bed_width),
        conductive_wire_channel_width(conductive_wire_channel_width),
        unrouted_wires(layer->unrouted_wires),
        grid_step_size(grid_step_size)
{
    // number of applicable perimeters
    this->max_perimeters = 9999;
    for(const auto &region : this->layer->regions) {
        this->max_perimeters = std::min(this->max_perimeters, region->region()->config.perimeters.value);
    }
    // ensure at least 1 perimeter for polygon offsetting
    this->max_perimeters = std::max(1, this->max_perimeters);

    // TODO: this should be coupled to the shell thickness!!!
    this->prev_shells = 3;
    this->next_shells = 4;
}

const coordf_t
ElectronicWireGenerator::get_print_z() const
{
    return this->layer->print_z;
}

const coord_t
ElectronicWireGenerator::get_scaled_print_z() const
{
    return scale_(this->layer->print_z);
}

const coordf_t
ElectronicWireGenerator::get_bottom_z() const
{
    return this->layer->print_z - this->layer->height;
}

const coordf_t
ElectronicWireGenerator::get_scaled_bottom_z() const
{
    return scale_(this->layer->print_z - this->layer->height);
}

/// return intersection of previous n slices to determine regions with solid material for wires
const ExPolygonCollection*
ElectronicWireGenerator::get_layer_slices() const
{
    return &(this->slices);
}

// generate set of deflated expolygons. Inflate by perimeter width per region and channel width.
ExPolygonCollections*
ElectronicWireGenerator::get_contour_set()
{
    if(this->deflated_slices.size() < 1) {

        // collect slices from n previous layers and compute intersection to
        // use only regions which are supported by at least n layers

        size_t region_count = this->layer->region_count();
        ExPolygonCollections slices;
        slices.resize(region_count);

        if(this->layer->lower_layer != nullptr && this->prev_shells > 0) {
            Layer* current_layer = this->layer->lower_layer;
            for(size_t r = 0; r < region_count; r++) {
                slices[r] = ExPolygonCollection(current_layer->regions[r]->slices);
            }
            for(int i = 1; i < this->prev_shells; i++) {
                if(current_layer->lower_layer != nullptr) {
                    current_layer = current_layer->lower_layer;
                    for(size_t r = 0; r < region_count; r++) {
                        slices[r] = intersection_ex((Polygons)slices[r], (Polygons)current_layer->regions[r]->slices);
                    }
                }
            }
        }else{
            for(size_t r = 0; r < region_count; r++) {
                slices[r] = ExPolygonCollection(this->layer->regions[r]->slices);
            }
        }

        // remove cavity polygons again on this layer as we don't want the wires running through components
        for(const Polygon &p : this->layer->electronic_component_polyons) {
            for(size_t r = 0; r < region_count; r++) {
                slices[r] = diff_ex((Polygons)slices[r], p);
            }
        }

        // handle next layers, coverage doesn't matter, only routed wires must be avoided
        ElectronicWireGenerator* current_ewg = this;
        for(int i = 0; i < this->next_shells; i++) {
            for(size_t r = 0; r < region_count; r++) {
                // routed wires
                for(Polyline &pl : current_ewg->routed_wires) {
                    slices[r] = diff_ex((Polygons)slices[r], this->inflate_wire(pl));
                }
                // cavities
                for(const Polygon &p : current_ewg->layer->electronic_component_polyons) {
                    for(size_t r = 0; r < region_count; r++) {
                        slices[r] = diff_ex((Polygons)slices[r], p);
                    }
                }
            }
            if(current_ewg->next_ewg != nullptr) {
                current_ewg = current_ewg->next_ewg;
            }else{
                break;
            }
        }

        for(int i=0; i < max_perimeters; i++) {
            // initialize vector element
            this->deflated_slices.push_back(ExPolygonCollection());

            //for(const auto &region : this->layer->regions) {
            //    const coord_t perimeters_thickness = this->offset_width(region, i+1);
            //    this->deflated_slices[i].append(offset_ex((Polygons)region->slices, -perimeters_thickness));
           // }

            for(int r = 0; r < this->layer->region_count(); r++) {
                const coord_t perimeters_thickness = this->offset_width(this->layer->regions[r], i+1);
                this->deflated_slices[i].append(offset_ex((Polygons)slices[r], -perimeters_thickness));
            }
            // intersection / offsetting introduces collinear points
            for(ExPolygon &ep : this->deflated_slices[i].expolygons) {
                ep.remove_colinear_points();
            }
        }

        // store slices in one collection for in/outside of material checking in graph search
        this->slices.expolygons.clear();
        for(ExPolygonCollection &epc : slices) {
            this->slices.append(epc);
        }

        this->_align_to_prev_perimeters();
    }

    return &this->deflated_slices;
}

/* Compute a set of weighed segments by intersecting
 * the given lines with all deflated contours.
 * Segments outside of the object get a higher weight (multiplied with routing_hole_factor).
 * Intersection points are added to the contour polygons.
 */
WireSegments
ElectronicWireGenerator::get_wire_segments(Lines& wire, const double routing_perimeter_factor, const double routing_hole_factor)
{
    // ensure deflated slices are properly initialized
    if(this->deflated_slices.size() < 1) {
        this->get_contour_set();
    }

    WireSegments segments;
    for(auto &line : wire) {
        WireSegment segment;
        segment.line = line;
        for(int current_perimeters = 0; current_perimeters < this->max_perimeters; current_perimeters++) {
            //double edge_weight_factor = 1.0 + (this->max_perimeters-current_perimeters-1) * routing_perimeter_factor;
            double edge_weight_factor = 1.0 + (this->max_perimeters-1) * routing_perimeter_factor;
            Line iteration_line = line;
            Line collision_line = line; // reset line for next iteration
            while(true) {
                bool outside_expolygon = false;
                Point intersection;
                bool ccw;
                Polygon* p;
                ExPolygon* ep;
                // find intersections with all expolygons
                while(this->deflated_slices[current_perimeters].first_intersection(collision_line, &intersection, &ccw, &p, &ep)) {
                    p->insert(intersection);
                    if(ccw) { // leaving expolygon
                        outside_expolygon = true;
                        Line l(iteration_line.a, intersection);
                        segment.connecting_lines.push_back(std::make_pair(l, edge_weight_factor));
                    }else{ // entering expolygon
                        Line l(iteration_line.a, intersection);
                        if(!outside_expolygon) {
                            segment.connecting_lines.push_back(std::make_pair(l, edge_weight_factor * routing_hole_factor));
                        }// else do nothing... we are only looking for connections inside polygons
                        outside_expolygon = false;
                    }
                    iteration_line.a = intersection;
                    collision_line.a = intersection;
                    collision_line.extend_start(-scale_(EPSILON)); // crop line a bit to avoid multiple intersections with the same polygon
                }
                // no further intersections found, rest must be inside or outside of slices. Append linear line
                if(this->deflated_slices[current_perimeters].contains_b(iteration_line.b)) {
                    // inside material, low weight
                    segment.connecting_lines.push_back(std::make_pair(iteration_line, edge_weight_factor));
                }else{
                    // outside material, high weight
                    segment.connecting_lines.push_back(std::make_pair(iteration_line, edge_weight_factor * routing_hole_factor));
                }
                break;
            }
        }
        segments.push_back(segment);
    }
    return segments;
}

// Add routed wire, generate channels, beds,
// resets local deflated_slices copy to trigger re-generation
void
ElectronicWireGenerator::add_routed_wire(Polyline &routed_wire)
{
    // generate channels / beds
    this->channel_from_wire(routed_wire);
    this->routed_wires.push_back(routed_wire);
    this->deflated_slices.clear();

    // also clear slices from affected prev / next layers
    ElectronicWireGenerator* current_ewg = this;
    for(int i = 0; i < this->prev_shells; i++) {
        if(current_ewg != nullptr) {
            current_ewg = current_ewg->next_ewg;
        }else{
            break;
        }
        if(current_ewg != nullptr) {
            current_ewg->deflated_slices.clear();
        }
    }

    current_ewg = this;
    for(int i = 0; i < this->next_shells; i++) {
        if(current_ewg != nullptr) {
            current_ewg = current_ewg->previous_ewg;
        }else{
            break;
        }
        if(current_ewg != nullptr) {
            current_ewg->deflated_slices.clear();
        }
    }
}

/* Generate a set of extrudable wires by sorting all segments
 * and finding connected sets. Extrusion lines end at intersections or endpoints.
 */
void
ElectronicWireGenerator::generate_wires()
{
    // find connected subsets
    std::list<Line> lines;
    // collect routed wires
    for(Polyline &pl : this->routed_wires) {
        Lines ls = pl.lines();
        std::copy(ls.begin(), ls.end(), std::back_inserter(lines));
    }

    this->routed_wires.clear();
    while(lines.size() > 0) {
        Polyline pl;
        // find an endpoint
        for (std::list<Line>::const_iterator line1 = lines.begin(); line1 != lines.end(); ++line1) {
            int hitsA = 0;
            int hitsB = 0;
            for (std::list<Line>::const_iterator line2 = lines.begin(); line2 != lines.end(); ++line2) {
                if(line1 == line2) {
                    continue;
                }
                if(line1->a.coincides_with_epsilon(line2->a) || line1->a.coincides_with_epsilon(line2->b)) {
                    hitsA++;
                }
                if(line1->b.coincides_with_epsilon(line2->a) || line1->b.coincides_with_epsilon(line2->b)) {
                    hitsB++;
                }
            }
            if(hitsA == 0) {
                pl.append(line1->a);
                pl.append(line1->b);
                lines.erase(line1);
                break;
            }
            if(hitsB == 0) {
                pl.append(line1->b);
                pl.append(line1->a);
                lines.erase(line1);
                break;
            }
        }

        if(pl.points.size() < 1) {
            std::cerr << "ERROR: routed wire has no endpoint! This should never happen, probably a loop." << std::endl;
            std::cerr << "remaining lines are: " << std::endl;
            for(Line &l : lines) {
                std::cerr << l.wkt() << std::endl;
            }
            // use first line as starting point
            pl.append(lines.front().a);
            pl.append(lines.front().b);
            lines.erase(lines.begin());
            //break;
        }

        // we now have an endpoint, traverse lines
        int hits = 1;
        while(hits == 1) {
            hits = 0;
            Point p = pl.last_point();
            std::list<Line>::iterator stored_line;
            Point stored_point;
            for (std::list<Line>::iterator line = lines.begin(); line != lines.end(); ++line) {
                if(p.coincides_with_epsilon(line->a)) {
                    hits++;
                    if(hits == 1) {
                        stored_point = line->b;
                        stored_line = line;
                        continue;
                    }
                }
                if(p.coincides_with_epsilon(line->b)) {
                    hits++;
                    if(hits == 1) {
                        stored_point = line->a;
                        stored_line = line;
                        continue;
                    }
                }
            }
            if(hits == 1) {
                pl.append(stored_point);
                lines.erase(stored_line);
            }
        }
        // slightly extend extrusion at intersections to improve connectivity with a small overlap
        if(hits > 1) {
            pl.extend_end(scale_(this->extrusion_width/2));
        }
        this->routed_wires.push_back(pl);
    }

    // cut wires and apply overlaps
    this->apply_overlap(this->routed_wires, &(this->layer->wires));
}

/* WARNING: legacy code! This is not ported for 3D-graph yet!
 */
void ElectronicWireGenerator::sort_unrouted_wires()
{
    // number of applicable perimeters
    int perimeters = 9999;
    for(const auto &region : this->layer->regions) {
      perimeters = std::min(perimeters, region->region()->config.perimeters.value);
    }
    coord_t wire_offset = this->offset_width(this->layer->regions.front(), perimeters);

    // generate channel expolygonCollection
    ExPolygonCollection deflated_wires;
    for(const auto &unrouted_wire : this->unrouted_wires) {
        // double offsetting for channels. 1 step generates a polygon from the polyline,
        // 2. step extends the polygon to avoid cropped angles.
        deflated_wires.append(offset_ex(offset(unrouted_wire, scale_(0.01)), wire_offset - scale_(0.01)));
    }

    // count intersections with channels created by other wires
    std::vector<unsigned int> intersections;
    for(const auto &unrouted_wire : this->unrouted_wires) {
        intersections.push_back(0);

        for(auto &line : unrouted_wire.lines()) {
            Point intersection;
            bool ccw;
            Polygon* p;
            ExPolygon* ep;
            while(deflated_wires.first_intersection(line, &intersection, &ccw, &p, &ep)) {
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
    channel_polygons = inflate_wire(wire);

    // remove beds and channels from layer
    for(const auto &region : this->layer->regions) {
        region->modify_slices(channel_polygons, false);
    }

    // if lower_layer is defined, use channel to create a "bed" by offsetting only a small amount
    // generate a bed by offsetting a small amount to trigger perimeter generation
    bed_polygons = offset(wire, scale_(this->conductive_wire_bed_width/2));
    if (this->layer->lower_layer != nullptr) {
        FOREACH_LAYERREGION(this->layer->lower_layer, layerm) {
            (*layerm)->modify_slices(bed_polygons, false);
        }
        this->layer->lower_layer->setDirty(true);
    }
    this->layer->setDirty(true);
}

// double offsetting for channels. 1 step generates a polygon from the polyline,
// 2. step extends the polygon to avoid cropped angles.
Polygons ElectronicWireGenerator::inflate_wire(const Polyline &wire) const {
    Polygons channel_polygons;
    channel_polygons = offset(wire, scale_(0.01));
    channel_polygons = offset(channel_polygons, scale_(this->extrusion_width/2 + this->conductive_wire_channel_width/2 - 0.01));
    return channel_polygons;
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
            //pl.clip_start(scale_(this->extrusion_width/2));
            //pl.clip_end(scale_(this->extrusion_width/2));

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

/* Check all points of this layers deflated perimeters against
 * previous layer and align if within epsilon distance
 * or project upwards from previous layer if within layer overlap tolerance.
 * After the projection step, the polygons are upsampled to
 * ensure a maximum distance between adjacent points < grid_step_distance.
 * WARNING: Currently very inefficient!!
 */
void
ElectronicWireGenerator::_align_to_prev_perimeters() {
    if(this->previous_ewg != nullptr) {
        // make sure previous ewg is already aligned
        this->previous_ewg->get_contour_set();

        //std::ostringstream ss;
        //ss << "alignment_orig_";
        //ss << this->layer->print_z;
        //ss << ".svg";
        //std::string filename = ss.str();
        //SVG svg_graph(filename.c_str());

        //svg_graph.draw(this->deflated_slices[0], "black");
        //svg_graph.draw((Points)this->deflated_slices[0], "red", scale_(0.6));

        //svg_graph.Close();

        //std::ostringstream ss1;
        //ss1 << "alignment_done_";
        //ss1 << this->layer->print_z;
        //ss1 << ".svg";
        //std::string filename1 = ss1.str();
        //SVG svg_graph1(filename1.c_str());

        //svg_graph1.draw(this->deflated_slices[0], "black");

        for(int i = 0; i < this->deflated_slices.size(); i++) {
            std::vector<Polygon*> polygons;
            for(auto &ep : this->deflated_slices[i].expolygons) {
                // contour
                polygons.push_back(&ep.contour);
                // holes
                for (Polygon &hole : ep.holes) {
                    polygons.push_back(&hole);
                }
            }
            if(this->previous_ewg->deflated_slices.size() > i) {
                // collect points from prev layer deflated slices
                Points prev_p = this->previous_ewg->deflated_slices[i];

                // iterate over all collected points from previous layer
                for(Point &other_p : prev_p) {
                    for(Polygon *poly : polygons) {
                        Point projection = other_p.projection_onto(*poly);
                        // TODO: also check threshold with layer height??
                        if(other_p.distance_to(projection) < scale_(this->extrusion_width/2)) {
                            // does this point already exist?
                            bool known_point = false;
                            for(Point &p : poly->points) {
                                // points match approximately, don't insert a new point
                                if(p.distance_to(other_p) < scale_(this->extrusion_width/2)) {
                                    known_point = true;
                                    if(p.coincides_with_epsilon(other_p)) { // points actually are the same
                                        p = other_p;
                                        if(i == 0) {
                                            //svg_graph1.draw(p, "green", scale_(0.8));
                                        }
                                        break;
                                    }
                                }
                            }
                            // insert it otherwise
                            if(!known_point) {
                                //std::cout << "inserting point. Nearest point was: " << min_distance << "scaled epsilon: " << SCALED_EPSILON << std::endl;
                                poly->insert(projection);
                                if(i == 0) {
                                    //svg_graph1.draw(projection, "blue", scale_(0.8));
                                }
                            }
                        }
                    }
                }
            }

            // insert new points if distance between adjacent points is too high
            for(Polygon *poly : polygons) {
                poly->upsample(this->grid_step_size); // TODO: this should be a configurable parameter! based on interlayer_overlap and grid step distance
            }
        }

        //svg_graph1.draw((Points)this->deflated_slices[0], "red", scale_(0.6));
        //svg_graph1.Close();
    }
}

}
