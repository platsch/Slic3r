#include "ElectronicRoutingGraph.hpp"

namespace Slic3r {

ElectronicRoutingGraph::ElectronicRoutingGraph()
{
    this->vertex_index = boost::get(&PointVertex::point, this->graph); // Index Vertex->Point
}

bool
ElectronicRoutingGraph::add_vertex(const Point3& p, routing_vertex_t* vertex)
{
    bool result = false;
    if(this->point_index.find(p) == this->point_index.end()) { // is this point already in the graph?
        *vertex = boost::add_vertex(this->graph);
        (this->graph)[*vertex].index = boost::num_vertices(this->graph)-1;
        (this->graph)[*vertex].point = p;
        (this->graph)[*vertex].predecessor = (*vertex);
        this->point_index[p] = *vertex; // add vertex to reverse lookup table
        this->rtree_index[p.z].insert((Point)p);
        result = true;
    }else{
        // return existing vertex
        *vertex = this->point_index[p];
    }
    return result;
}

bool
ElectronicRoutingGraph::add_vertex(const Point3& p)
{
    routing_vertex_t v;
    return this->add_vertex(p, &v);
}

bool
ElectronicRoutingGraph::add_edge(const Line3& l, routing_edge_t* edge, const double& edge_weight_factor)
{
    bool result = true;
    try {
        routing_vertex_t vertex_a = this->point_index.at(l.a); // unordered_map::at throws an out-of-range
        routing_vertex_t vertex_b = this->point_index.at(l.b); // unordered_map::at throws an out-of-range
        coord_t distance = l.length() * edge_weight_factor;
        *edge = boost::add_edge(vertex_a, vertex_b, distance, this->graph).first;
    } catch (const std::out_of_range& oor) {
        std::cerr << "Unable to find vertex for point while adding an edge: " << oor.what() << '\n';
        std::cout << "Line was: " << l << std::endl;
        result = false;
    }
    return result;
}

void
ElectronicRoutingGraph::add_polygon(const Polygon& p, const double& weight_factor, const coord_t print_z)
{
    if(p.is_valid()) {
        routing_vertex_t last_vertex, this_vertex, first_vertex;
        this->add_vertex(Point3(p.points.front(), print_z) , &last_vertex); // add first point
        first_vertex = last_vertex;
        for (Points::const_iterator it = p.points.begin(); it != p.points.end()-1; ++it) {
            // add next vertex
            this->add_vertex(Point3(*(it+1), print_z), &this_vertex);

            // add edge
            coord_t distance = it->distance_to(*(it+1)) * weight_factor;
            // Add factor to prioritize routing with high distance to external perimeter
            boost::add_edge(last_vertex, this_vertex, distance, this->graph);
            last_vertex = this_vertex;
        }
        // add final edge to close the loop
        coord_t distance = p.points.back().distance_to(p.points.front()) * weight_factor;
        // Add factor to prioritize routing with high distance to external perimeter
        boost::add_edge(last_vertex, first_vertex, distance, this->graph);
    }
}

bool
ElectronicRoutingGraph::nearest_point(const Point3& dest, Point3* point)
{
    bool result = false;
    if(std::find(this->z_positions.begin(), this->z_positions.end(), dest.z) != this->z_positions.end()) {
        std::vector<Point> query_result;
        this->rtree_index[dest.z].query(boost::geometry::index::nearest((Point)dest, 1), std::back_inserter(query_result));
        if(query_result.size() > 0) {
            *point = Point3(query_result.front(), dest.z);
            result = true;
        }else{
            std::cerr << "WARNING: point " << dest << " is not in spatial index!" << std::endl;
        }
    }
    return result;
}

bool
ElectronicRoutingGraph::points_in_boxrange(const Point3& dest, const coord_t range, Points* points)
{
    bool result = false;
    if(std::find(this->z_positions.begin(), this->z_positions.end(), dest.z) != this->z_positions.end()) {
        boost::geometry::model::box<Point> box(Point(dest.x-range, dest.y-range), Point(dest.x+range, dest.y+range));
        this->rtree_index[dest.z].query(boost::geometry::index::within(box), std::back_inserter(*points));
        if(points->size() > 0) {
            result = true;
        }else{
            std::cerr << "WARNING: point " << dest << " is not in spatial index!" << std::endl;
        }
    }
    return result;
}

void
ElectronicRoutingGraph::append_z_position(coord_t z)
{
    this->z_positions.push_back(z);
    this->rtree_index[z] = spatial_index_t();
}

bool
ElectronicRoutingGraph::astar_route(
        const Point3& start_p,
        const Point3& goal_p,
        PolylinesMap* route_map,
        const double routing_explore_weight_factor,
        const double routing_interlayer_factor,
        const double grid_step_size,
        const double layer_overlap,
        const double overlap_tolerance,
        const double astar_factor
        )
{
    bool result = false;

    routing_vertex_t start = this->point_index[start_p];
    routing_vertex_t goal = this->point_index[goal_p];

    std::cout << "start: " << start_p << " goal: " << goal_p << std::endl;

    // generate required maps for A* search
    boost::property_map<routing_graph_t, routing_vertex_t PointVertex::*>::type predmap = boost::get(&PointVertex::predecessor, this->graph);
    // init maps
    boost::graph_traits<routing_graph_t>::vertex_iterator i, iend;
    unsigned int idx = 0;
    for (boost::tie(i, iend) = boost::vertices(this->graph); i != iend; ++i, ++idx) {
        predmap[*i] = *i;
        this->graph[*i].index = idx;
        this->graph[*i].distance = INF;
        this->graph[*i].color = boost::white_color;
    }

    // A* visitor
    astar_visitor<routing_graph_t, routing_vertex_t, routing_edge_t, ElectronicRoutingGraph, ExpolygonsMap> vis(
            goal,
            this,
            this->slices_surfaces_map,
            this->infill_surfaces_map,
            this->z_positions,
            &this->interlayer_overlaps,
            grid_step_size,
            &this->point_index,
            routing_explore_weight_factor,
            routing_interlayer_factor,
            layer_overlap,
            overlap_tolerance);
    // distance heuristic
    distance_heuristic<routing_graph_t, coord_t, boost::property_map<routing_graph_t, Point3 PointVertex::*>::type> dist_heuristic(this->vertex_index, goal, astar_factor);

    // init start vertex
    this->graph[start].distance = 0;
    this->graph[start].cost = dist_heuristic(start);

    try {
        // call astar named parameter interface
        astar_search_no_init
        (this->graph, start,
            dist_heuristic,
            vertex_index_map(boost::get(&PointVertex::index, this->graph)).
            predecessor_map(predmap).
            color_map(boost::get(&PointVertex::color, this->graph)).
            distance_map(boost::get(&PointVertex::distance, this->graph)).
            rank_map( boost::get(&PointVertex::cost, this->graph)).
            visitor(vis).
            distance_compare(std::less<coord_t>()).
            distance_combine(boost::closed_plus<coord_t>(INF)).
            distance_inf(INF)
            );
    } catch(found_goal fg) { // found a path to the goal
        std::cout << "FOUND GOAL: " << this->vertex_index[goal] << std::endl;
        std::cout << "nr of vertices: " << boost::num_vertices(this->graph) << std::endl;
        coord_t last_z = this->vertex_index[goal].z;
        routing_vertex_t last_v;
        Polyline routed_wire;
        for(routing_vertex_t v = goal;; v = predmap[v]) {
            //std::cout << "goal to start: " << this->vertex_index[v] << std::endl;
            if(predmap[v] == v) {
                routed_wire.append((Point)this->vertex_index[v]);
                break;
            }

            // layer change
            if(last_z != this->vertex_index[v].z) {

                // apply interlayer overlaps
                routing_edge_t edge = boost::edge(v, last_v, this->graph).first;

                Polyline last_overlap, this_overlap;
                try {
                   std::pair<Polyline, Polyline> overlap = interlayer_overlaps.at(edge); // map::at throws an out-of-range
                   if(last_z < this->vertex_index[v].z) {
                       last_overlap = overlap.first;
                       this_overlap = overlap.second;
                   }else{
                       last_overlap = overlap.second;
                       this_overlap = overlap.first;
                   }
               } catch (const std::out_of_range& oor) {
                   std::cerr << "Unable to find overlap for edge: " << oor.what() << '\n';
                   std::cout << "Edge was: " << edge << std::endl;
               }

                // correct direction?
                if(!last_overlap.first_point().coincides_with_epsilon(routed_wire.last_point())) {
                    last_overlap.reverse();
                    this_overlap.reverse();
                }

                for (Points::iterator p = last_overlap.points.begin()+1; p != last_overlap.points.end(); ++p) {
                    routed_wire.append(*p);
                }
                (*route_map)[last_z].push_back(routed_wire);
                routed_wire.points.clear();

                for (Points::iterator p = this_overlap.points.begin(); p != this_overlap.points.end()-1; ++p) {
                    routed_wire.append(*p);
                }
            }
            last_z = this->vertex_index[v].z;
            last_v = v;
            routed_wire.append((Point)this->vertex_index[v]);
        }

        (*route_map)[last_z].push_back(routed_wire);
        result = true;
    }

    return result;
}

void
ElectronicRoutingGraph::fill_svg(SVG* svg, const coord_t z, const ExPolygonCollection& s, const routing_vertex_t& current_v) const
{
    //background (infill)
    svg->draw(s, "black");
    boost::graph_traits<routing_graph_t>::edge_iterator ei, ei_end;
    for (boost::tie(ei, ei_end) = edges(this->graph); ei != ei_end; ++ei) {
        if(this->graph[boost::source(*ei, this->graph)].point.z == z || this->graph[boost::target(*ei, this->graph)].point.z == z) {
            Line l((Point)this->graph[boost::source(*ei, this->graph)].point, (Point)this->graph[boost::target(*ei, this->graph)].point);
            boost::property_map<routing_graph_t, boost::edge_weight_t>::const_type EdgeWeightMap = boost::get(boost::edge_weight_t(), this->graph);
            coord_t weight = boost::get(EdgeWeightMap, *ei);
            double w = weight / l.length();
            int w_int = std::min((int)((w-1)*200), 255);
            std::ostringstream ss;
            ss << "rgb(" << 100 << "," << w_int << "," << w_int << ")";
            std::string color = ss.str();

            // interlayer-connections
            if(this->graph[boost::source(*ei, this->graph)].point.z != this->graph[boost::target(*ei, this->graph)].point.z) {
                color = "green";
            }

            //std::cout << "color: " << color << std::endl;
            //if(g[boost::source(*ei, g)].color == boost::white_color) {
            //    svg_graph.draw(l,  "white", scale_(0.1));
            //}else{
            //    svg_graph.draw(l,  "blue", scale_(0.1));
            //}
            svg->draw(l,  color, scale_(0.1));
        }
    }

    // draw current route
    if(current_v) {
        auto predmap = boost::get(&PointVertex::predecessor, this->graph);
        Point last_point = (Point)this->graph[current_v].point;
        for(routing_vertex_t v = current_v;; v = predmap[v]) {
            Line l = Line(last_point, (Point)this->graph[v].point);
            svg->draw(l,  "blue", scale_(0.07));
            last_point = (Point)this->graph[v].point;
            if(predmap[v] == v)
                break;
        }
        // highlight current vertex
        svg->draw((Point)this->graph[current_v].point, "blue", scale_(0.15));
    }
}

void
ElectronicRoutingGraph::write_svg(const std::string filename, const coord_t z) const
{
    ExpolygonsMap surfaces = this->infill_surfaces_map;
    if(((Points)(*surfaces[z])).size() > 3) {
        BoundingBox bb = surfaces[z]->convex_hull().bounding_box();
        bb.offset(scale_(5));
        SVG svg(filename.c_str(), bb);
        this->fill_svg(&svg, z);
        svg.Close();
    }
}

}
