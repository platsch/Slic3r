#include "ElectronicWireGenerator.hpp"
#include  "SVG.hpp"

namespace Slic3r {

ElectronicWireRouter::ElectronicWireRouter(const double layer_overlap, const double routing_perimeter_factor,
        const double routing_hole_factor, const double routing_interlayer_factor, const int layer_count) :
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
    routing_graph_t routing_graph;
    boost::property_map<routing_graph_t, Point3 PointVertex::*>::type pointmap = boost::get(&PointVertex::point, routing_graph); // Index Vertex->Point
    point_index_t graph_point_index; // Index Point->Vertex

    int index = 0;

    // store pointer to infill area polygons for vertex generation in A*
    ExpolygonsMap infill_surfaces_map;
    std::vector<coord_t> z_positions;
    InterlayerOverlaps interlayer_overlaps;

    int max_perimeters = 0;

    // (re-)build routing graph
    Lines wire, last_wire;
    bool overlap_wire_a , overlap_wire_b, last_overlap_wire_a, last_overlap_wire_b;
    for (auto &ewg : this->ewgs) {
        coord_t print_z = ewg.get_scaled_print_z();
        coord_t bottom_z = ewg.get_scaled_bottom_z();
        z_positions.push_back(print_z);
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
            wire_segments = ewg.get_wire_segments(wire, this->routing_hole_factor);
            // Add connecting lines to graph!!!
        }
        ExPolygonCollections* contours = ewg.get_contour_set();

        // add contour polygons to graph
        for(int i = 0; i < ewg.max_perimeters; i++) {
            double edge_weight_factor = 1.0 + (ewg.max_perimeters-i-1) * this->routing_perimeter_factor;
            for(ExPolygon &ep : (ExPolygons)(*contours)[i]) {
                // iterate over contour and holes to avoid deep copy
                Slic3r::polygon_to_graph(ep.contour, &routing_graph, &graph_point_index, edge_weight_factor, print_z);
                for(Polygon &p : ep.holes) {
                    Slic3r::polygon_to_graph(p, &routing_graph, &graph_point_index, edge_weight_factor, print_z);
                }
            }
         }
        infill_surfaces_map[print_z] = &(contours->back());

/*        // debug output graph
        //SVG svg_graph("graph_visitor");
        std::ostringstream ss1;
        ss1 << "base_contours_";
        ss1 << print_z;
        ss1 << ".svg";
        std::string filename1 = ss1.str();
        SVG svg_graph1(filename1.c_str());
        svg_graph1.draw(*(ewg.slices), "black");
        boost::graph_traits<routing_graph_t>::edge_iterator ei, ei_end;
        for (boost::tie(ei, ei_end) = edges(routing_graph); ei != ei_end; ++ei) {
            if(routing_graph[boost::source(*ei, routing_graph)].point.z == print_z || routing_graph[boost::target(*ei, routing_graph)].point.z == print_z) {
                Line l = Line((Point)routing_graph[boost::source(*ei, routing_graph)].point, (Point)routing_graph[boost::target(*ei, routing_graph)].point);
                boost::property_map<routing_graph_t, boost::edge_weight_t>::type EdgeWeightMap = boost::get(boost::edge_weight_t(), routing_graph);
                coord_t weight = boost::get(EdgeWeightMap, *ei);
                double w = weight / l.length();
                w -=0.99;
                w = w*1000;
                //std::cout << "w for debug: " << w << std::endl;
                int w_int = std::min((int)w, 255);
                std::ostringstream ss;
                ss << "rgb(" << 100 << "," << w_int << "," << w_int << ")";
                std::string color = ss.str();
                //std::cout << "color: " << color << std::endl;
                //if(g[boost::source(*ei, g)].color == boost::white_color) {
                //    svg_graph.draw(l,  "white", scale_(0.1));
                //}else{
                //    svg_graph.draw(l,  "blue", scale_(0.1));
                //}
                svg_graph1.draw(l,  color, scale_(0.2));
            }
        }*/


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
                Slic3r::add_point_vertex(Point3(l.a, print_z), routing_graph, graph_point_index);
                Slic3r::add_point_vertex(Point3(l.b, print_z), routing_graph, graph_point_index);
                Slic3r::add_point_edge(Line3(l, print_z), routing_graph, graph_point_index, &edge, edge_weight_factor);
                segment.edges.insert(edge);

 //               Line dbg = l;
 //               dbg.translate(scale_(connecting_l_offset), 0);
 //               std::ostringstream ss;
 //               ss << "rgb(" << 100 << "," << edge_weight_factor*100 << "," << edge_weight_factor*100 << ")";
 //               std::string color = ss.str();
                //svg_graph1.draw(dbg, color, scale_(0.2));
 //               float debug_offset = 0.0;

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
 //                       std::ostringstream ss;
 //                       ss << "rgb(" << 100 << "," << edge_weight_factor*100 << "," << edge_weight_factor*100 << ")";
 //                       std::string color = ss.str();

                        // add points of 2nd line
                        Slic3r::add_point_vertex(Point3(segment.connecting_lines[j].first.a, print_z), routing_graph, graph_point_index);
                        Slic3r::add_point_vertex(Point3(segment.connecting_lines[j].first.b, print_z), routing_graph, graph_point_index);
                        // add the remaining combination of edges. 2nd line will be added later in the loop.
                        Slic3r::add_point_edge(Line3(segment.connecting_lines[i].first.a, segment.connecting_lines[j].first.b, print_z), routing_graph, graph_point_index, &edge, edge_weight_factor);
                        segment.edges.insert(edge);

                        Line d = Line(segment.connecting_lines[i].first.a, segment.connecting_lines[j].first.b);
 //                       d.translate(scale_(connecting_l_offset+debug_offset+0.15), 0);
 //                       //svg_graph1.draw(d, color, scale_(0.1));

                        Slic3r::add_point_edge(Line3(segment.connecting_lines[j].first.a, segment.connecting_lines[i].first.b, print_z), routing_graph, graph_point_index, &edge, edge_weight_factor);
                        segment.edges.insert(edge);

 //                       d = Line(segment.connecting_lines[j].first.a, segment.connecting_lines[i].first.b);
 //                       d.translate(scale_(connecting_l_offset+debug_offset+0.3), 0);
                        //svg_graph1.draw(d, color, scale_(0.1));
 //                       debug_offset += 0.4;
                    }
                }
//                if(l.b.coincides_with_epsilon(segment.line.b)) {
//                    connecting_l_offset += 1;
//                }
//                if(std::abs(segment.line.a.y - segment.line.b.y) < 1000) {
//                    connecting_l_offset = 0;
 //               }
            }
        }
 //       svg_graph1.Close();

        // add direct z-interconnects to the graph
        if(last_wire.size() > 0 && wire.size() > 0) {
            double edge_weight_factor = 1.0 + (ewg.max_perimeters-1) * this->routing_perimeter_factor;
            Point3 a, b;
            bool interconnect = false;
            if(overlap_wire_a && last_overlap_wire_b) {
                a = Point3(last_wire.back().b, print_z);
                b = Point3(wire.front().a, bottom_z);
                interconnect = true;
            }
            if(overlap_wire_b && last_overlap_wire_a) {
                a = Point3(last_wire.front().a, print_z);
                b = Point3(wire.back().b, bottom_z);
                interconnect = true;
            }
            if(interconnect) {
                routing_edge_t edge;

                // This is very inefficient!!!
                boost::graph_traits<routing_graph_t>::vertex_iterator k, kend;
                unsigned int idx = 0;
                for (boost::tie(k, kend) = boost::vertices(routing_graph); k != kend; ++k) {
                    if(a.coincides_with_epsilon(routing_graph[*k].point)) {
                        a = routing_graph[*k].point;
                    }
                    if(b.coincides_with_epsilon(routing_graph[*k].point)) {
                        b = routing_graph[*k].point;
                    }
                }

                //Slic3r::add_point_edge(Line3(a, b), routing_graph, graph_point_index, &edge, this->routing_interlayer_factor);
                coord_t distance = ((Point)a).distance_to((Point)b) * edge_weight_factor;
                if(!contours->back().contains_b(a) || !contours->back().contains_b(b)) {
                    distance = distance * this->routing_hole_factor;
                }
                distance += scale_(this->routing_interlayer_factor);
                edge = boost::add_edge(graph_point_index.at(a), graph_point_index.at(b), distance, routing_graph).first;
                Polyline pl;
                pl.append((Point)a);
                pl.append((Point)b);
                interlayer_overlaps[edge] = pl;
            }
        }

        last_wire = wire;
        last_overlap_wire_a = overlap_wire_a;
        last_overlap_wire_b = overlap_wire_b;

  /*      // debug output graph
       std::ostringstream ss;
       ss << "graph_debug_";
       ss << index;
       ss << ".svg";
       std::string filename = ss.str();
       SVG svg_graph(filename.c_str());
       svg_graph.draw(*(ewg.slices), "black");
       boost::graph_traits<routing_graph_t>::edge_iterator ei, ei_end;
       for (boost::tie(ei, ei_end) = edges(routing_graph); ei != ei_end; ++ei) {
           if(routing_graph[boost::source(*ei, routing_graph)].point.z >= print_z && routing_graph[boost::target(*ei, routing_graph)].point.z >= print_z) {
               Point a = (Point)pointmap[boost::source(*ei, routing_graph)];
               Point b = (Point)pointmap[boost::target(*ei, routing_graph)];
               Line l = Line(a, b);
               svg_graph.draw(l, "red", scale_(0.1));
           }
       }

       svg_graph.arrows = false;
       svg_graph.Close();*/
       index++;
    }

    // A* search on routing graph

    // generate required maps for A* search
    boost::property_map<routing_graph_t, routing_vertex_t PointVertex::*>::type predmap = boost::get(&PointVertex::predecessor, routing_graph);
    // init maps
    boost::graph_traits<routing_graph_t>::vertex_iterator i, iend;
    unsigned int idx = 0;
    for (boost::tie(i, iend) = boost::vertices(routing_graph); i != iend; ++i, ++idx) {
        predmap[*i] = *i;
        routing_graph[*i].index = idx;
        routing_graph[*i].distance = INF;
        routing_graph[*i].color = boost::white_color;
    }

    // start / goal vertex
    Line3s lines = rb->getExtendedSegmets();
    for(auto &l : lines) {
        l.translate(offset.x, offset.y, 0);
        // map to next layer boundary
        l.a.z = this->_map_z_to_layer(l.a.z);
        l.b.z = this->_map_z_to_layer(l.b.z);
    }

    routing_vertex_t start = graph_point_index[lines.front().a];
    routing_vertex_t goal = graph_point_index[lines.back().b];
    std::cout << "start: " << lines.front().a << std::endl;
    std::cout << "goal: " << lines.back().b << std::endl;

    // A* visitor
    astar_visitor<routing_graph_t, routing_vertex_t, ExpolygonsMap> vis(
            goal,
            infill_surfaces_map,
            z_positions,
            &interlayer_overlaps,
            scale_(1),
            &graph_point_index,
            (1.0 + max_perimeters * this->routing_perimeter_factor),
            this->routing_interlayer_factor,
            this->layer_overlap);
    // distance heuristic
    distance_heuristic<routing_graph_t, coord_t, boost::property_map<routing_graph_t, Point3 PointVertex::*>::type> dist_heuristic(pointmap, goal);
    // init start vertex
    routing_graph[start].distance = 0;
    routing_graph[start].cost = dist_heuristic(start);

    try {
        // call astar named parameter interface
        astar_search_no_init
        (routing_graph, start,
            dist_heuristic,
            vertex_index_map(boost::get(&PointVertex::index, routing_graph)).
            predecessor_map(predmap).
            color_map(boost::get(&PointVertex::color, routing_graph)).
            distance_map(boost::get(&PointVertex::distance, routing_graph)).
            rank_map( boost::get(&PointVertex::cost, routing_graph)).
            visitor(vis).
            distance_compare(std::less<coord_t>()).
            distance_combine(boost::closed_plus<coord_t>(INF)).
            distance_inf(INF)
            );
    } catch(found_goal fg) { // found a path to the goal
        std::cout << "FOUND GOAL!!!" << std::endl;
        coord_t last_z = pointmap[goal].z;
        routing_vertex_t last_v;
        Polyline routed_wire;
        for(routing_vertex_t v = goal;; v = predmap[v]) {
            if(predmap[v] == v) {
                routed_wire.append((Point)pointmap[v]);
                break;
            }

            // layer change
            if(last_z != pointmap[v].z) {
                // apply interlayer overlaps
                routing_edge_t edge = boost::edge(v, last_v, routing_graph).first;
                Polyline overlap = interlayer_overlaps[edge];
                // correct direction?
                if(!overlap.first_point().coincides_with_epsilon(routed_wire.last_point())) {
                    overlap.reverse();
                }
                for (Points::iterator p = overlap.points.begin()+1; p != overlap.points.end(); ++p) {
                    routed_wire.append(*p);
                }
                this->z_map[last_z]->add_routed_wire(routed_wire);
                routed_wire.points.clear();

                for (Points::iterator p = overlap.points.begin(); p != overlap.points.end()-1; ++p) {
                    routed_wire.append(*p);
                }
            }
            last_z = pointmap[v].z;
            last_v = v;
            routed_wire.append((Point)pointmap[v]);
        }
        this->z_map[last_z]->add_routed_wire(routed_wire);
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
