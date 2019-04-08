#include "ElectronicWireGenerator.hpp"
#include  "SVG.hpp"

namespace Slic3r {

ElectronicWireRouter::ElectronicWireRouter(const double layer_overlap, const double routing_astar_factor,
        const double routing_perimeter_factor, const double routing_hole_factor,
        const double routing_interlayer_factor, const int layer_count) :
        routing_astar_factor(routing_astar_factor),
        routing_perimeter_factor(routing_perimeter_factor),
        routing_hole_factor(routing_hole_factor),
        routing_interlayer_factor(routing_interlayer_factor),
        layer_overlap(layer_overlap){
    this->ewgs.reserve(layer_count + 1); // required to avoid reallocations -> invalid previous_ewg references
}

void
ElectronicWireRouter::append_wire_generator(ElectronicWireGenerator& ewg)
{
    this->ewgs.push_back(ewg);
    this->z_map[ewg.get_scaled_print_z()] = &ewgs.back();
}

ElectronicWireGenerator*
ElectronicWireRouter::last_ewg() {
    if(this->ewgs.size() > 0) {
        return &this->ewgs.back();
    }
    return NULL;
}

void
ElectronicWireRouter::route(const RubberBand* rb, const Point3 offset)
{
    // setup graph structure for routing
    ElectronicRoutingGraph graph;

    int max_perimeters = 0;

    // (re-)build routing graph
    Lines wire, last_wire;
    bool overlap_wire_a , overlap_wire_b;
    for (auto &ewg : this->ewgs) {
        coord_t print_z = ewg.get_scaled_print_z();
        coord_t bottom_z = ewg.get_scaled_bottom_z();
        graph.append_z_position(print_z);
        WireSegments wire_segments;
        max_perimeters = std::max(max_perimeters, ewg.max_perimeters);
        // get flat segments for this layer, including pin contact extensions
        wire.clear();
        if(rb->getLayerSegments(ewg.get_bottom_z(), ewg.get_print_z(), this->layer_overlap, &wire, &overlap_wire_a, &overlap_wire_b)) {
            // electronics are in (0, 0) coordinate system, object in (center, center).
            for(auto &w : wire) {
                w.translate(offset.x, offset.y);
            }
            // this MUST be done before get_contour_set() since it adds points to the
            // polygons at intersections!!
            wire_segments = ewg.get_wire_segments(wire, this->routing_perimeter_factor, this->routing_hole_factor);
            // Add connecting lines to graph!!!
        }
        ExPolygonCollections* contours = ewg.get_contour_set();

        // add contour polygons to graph
        for(int i = 0; i < ewg.max_perimeters; i++) {
            double edge_weight_factor = 1.0 + (ewg.max_perimeters-i-1) * this->routing_perimeter_factor;
            for(ExPolygon &ep : (ExPolygons)(*contours)[i]) {
                // iterate over contour and holes to avoid deep copy
                graph.add_polygon(ep.contour, edge_weight_factor, print_z);
                for(Polygon &p : ep.holes) {
                    graph.add_polygon(p, edge_weight_factor, print_z);
                }
            }
         }
        graph.infill_surfaces_map[print_z] = &(contours->back());
        graph.slices_surfaces_map[print_z] = ewg.get_layer_slices();

        // debug output graph
        //SVG svg_graph("graph_visitor");
//        std::ostringstream ss1;
//        ss1 << "base_contours_";
//        ss1 << print_z;
//        ss1 << ".svg";
//        std::string filename1 = ss1.str();
//        SVG svg_graph1(filename1.c_str());
//        graph.fill_svg(&svg_graph1, print_z, *(ewg.get_layer_slices()));

        // add wire segments to the graph
        for(auto &segment : wire_segments) {

//            svg_graph1.draw(segment.line, "blue", scale_(0.1));
            float connecting_l_offset = 1;
            if(std::abs(segment.line.a.y - segment.line.b.y) < 1000) {
                connecting_l_offset = 0;
            }

            // add connecting lines to graph
            for(int i=0; i<segment.connecting_lines.size(); i++) {
                double edge_weight_factor = segment.connecting_lines[i].second;

                // add endpoints to graph. Won't add multiple copies of the same point due to hash structure.
                routing_edge_t edge;
                Line l = segment.connecting_lines[i].first;
                graph.add_vertex(Point3(l.a, print_z));
                graph.add_vertex(Point3(l.b, print_z));
                graph.add_edge(Line3(l, print_z), &edge, edge_weight_factor);
                segment.edges.insert(edge);

                Line dbg = l;
                //dbg.translate(scale_(connecting_l_offset), 0);
                std::ostringstream ss;
                int w = std::min((int)((edge_weight_factor-1)*1000), 255);
                ss << "rgb(" << 100 << "," << w << "," << w << ")";
                std::string color = ss.str();
//                svg_graph1.draw(dbg, color, scale_(0.2));
                float debug_offset = 0.0;

                for(int j=i+1; j<segment.connecting_lines.size(); j++) {
                    bool add_overlaps = false;
                    if(l.contains(segment.connecting_lines[j].first)) {
                        edge_weight_factor = segment.connecting_lines[i].second;
                        add_overlaps = true;
                    }
                    if(segment.connecting_lines[j].first.contains(l)) {
                        edge_weight_factor = segment.connecting_lines[j].second;
                        add_overlaps = true;
                    }
                    if(add_overlaps) {
                        std::ostringstream ss;
                        w = std::min((int)((edge_weight_factor-1)*1000), 255);
                        ss << "rgb(" << 100 << "," << w << "," << w << ")";
                        std::string color = ss.str();

                        // add points of 2nd line
                        graph.add_vertex(Point3(segment.connecting_lines[j].first.a, print_z));
                        graph.add_vertex(Point3(segment.connecting_lines[j].first.b, print_z));
                        // add the remaining combination of edges. 2nd line will be added later in the loop.
                        graph.add_edge(Line3(segment.connecting_lines[i].first.a, segment.connecting_lines[j].first.b, print_z), &edge, edge_weight_factor);
                        segment.edges.insert(edge);

                        Line d = Line(segment.connecting_lines[i].first.a, segment.connecting_lines[j].first.b);
                        //d.translate(scale_(connecting_l_offset+debug_offset+0.15), 0);
//                        svg_graph1.draw(d, color, scale_(0.1));

                        graph.add_edge(Line3(segment.connecting_lines[j].first.a, segment.connecting_lines[i].first.b, print_z), &edge, edge_weight_factor);
                        segment.edges.insert(edge);

                        d = Line(segment.connecting_lines[j].first.a, segment.connecting_lines[i].first.b);
                        //d.translate(scale_(connecting_l_offset+debug_offset+0.3), 0);
//                        svg_graph1.draw(d, color, scale_(0.1));
                        debug_offset += 0.4;
                    }
                }
                if(l.b.coincides_with_epsilon(segment.line.b)) {
                    connecting_l_offset += 1;
                }
                if(std::abs(segment.line.a.y - segment.line.b.y) < 1000) {
                    connecting_l_offset = 0;
                }
            }
        }
        //svg_graph1.Close();

        // add direct z-interconnects to the graph
        if(last_wire.size() > 0 && wire.size() > 0) {
            double edge_weight_factor = 1.0 + (ewg.max_perimeters-1) * this->routing_perimeter_factor;
            Point3 a, b;
            bool interconnect = false;
            // overlapping wires
            if((last_wire.front()).contains(wire.back().b) && (wire.back().contains(last_wire.front().a))) {
                a = Point3(wire.back().b, bottom_z);
                b = Point3(last_wire.front().a, print_z);
                interconnect = true;
            }
            if((last_wire.back()).contains(wire.front().a) && (wire.front().contains(last_wire.back().b))) {
                a = Point3(wire.front().a, bottom_z);
                b = Point3(last_wire.back().b, print_z);
                interconnect = true;
            }

            if(interconnect) {
                routing_edge_t edge;

                // use nearest point to mitigate numerical instability
                if(graph.nearest_point(a, &a) &&
                   graph.nearest_point(b, &b)) {

//                    svg_graph1.draw(last_wire, "yellow", scale_(0.2));
//                    svg_graph1.draw(wire, "blue", scale_(0.15));
//                    svg_graph1.draw((Point)b, "green", scale_(0.1));


                    coord_t distance = ((Point)a).distance_to((Point)b) * edge_weight_factor;
                    if(!contours->back().contains_b(a) || !contours->back().contains_b(b)) {
                        distance = distance * this->routing_hole_factor;
                    }
                    distance += scale_(this->routing_interlayer_factor);

                    // use the helper function to catch map misses
                    Line3 l(a, b);
                    double edge_weight_factor = distance/l.length();
                    graph.add_edge(l, &edge, edge_weight_factor);

                    Polyline pl;
                    pl.append((Point)a);
                    pl.append((Point)b);
                    graph.interlayer_overlaps[edge] = pl;
                }else{
                    std::cerr << "WARNING: unable to add direct interlayer interconnect to routing graph! skipping this connection..." << std::endl;
                }
            }
        }

        last_wire = wire;

//        svg_graph1.Close();
    }

    // A* search on routing graph
    // start / goal vertex
    Line3s lines = rb->getExtendedSegmets();
    for(auto &l : lines) {
        l.translate(offset.x, offset.y, 0);
        // map to next layer boundary
        l.a.z = this->_map_z_to_layer(l.a.z);
        l.b.z = this->_map_z_to_layer(l.b.z);
    }

    Point3 start = lines.front().a;
    Point3 goal = lines.back().b;
    PolylinesMap route_map;

    if(graph.astar_route(
            start,
            goal,
            &route_map,
            (1.0 + max_perimeters * this->routing_perimeter_factor),
            this->routing_interlayer_factor,
            this->layer_overlap,
            this->routing_astar_factor))
    {
        // push resulting routes to EWG objects
        for (std::pair<coord_t, Polylines> it : route_map) {
            for (Polyline& pl : it.second) {
                this->z_map[it.first]->add_routed_wire(pl);
            }
        }
        //this->z_map[last_z]->add_routed_wire(routed_wire);
    } else {
        // SHOW WARNING or do something else to inform user!
    }

    // remove edges generated by this segment to avoid double usage when routing the next segment
    //for(auto edge : segment.edges) {
    //    boost::remove_edge(edge, routing_graph);
    //}
}


void
ElectronicWireRouter::generate_wires()
{
    for(auto &ewg : this->ewgs) {
        ewg.generate_wires();
    }
}

/* map z-coordinate to
 * next print_z to align to layer boundaries.
 * all values in scaled coordinates.
 */
coord_t
ElectronicWireRouter::_map_z_to_layer(coord_t z) const
{
    coord_t result = 0;
    for(auto &ewg : this->ewgs) {
        result = ewg.get_scaled_print_z();
        if(ewg.get_scaled_print_z() > z) {
            break;
        }
    }
    return result;
}

}
