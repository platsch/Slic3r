#ifndef slic3r_ElectronicRoutingGraph_hpp_
#define slic3r_ElectronicRoutingGraph_hpp_

#include "Polygon.hpp"
#include  "SVG.hpp"

#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/graph_traits.hpp>
#include <boost/graph/graphviz.hpp>
//#include <boost/graph/astar_search.hpp>
#include "astar_search.hpp"

#include <boost/geometry.hpp>
#include <boost/geometry/geometries/register/point.hpp>

// Define translation between Slic3r::Point and boost::geometry::model::point
BOOST_GEOMETRY_REGISTER_POINT_2D(Slic3r::Point, coord_t, cs::cartesian, x, y)

namespace Slic3r {

class ElectronicRoutingGraph;

// Define boost graph types
struct PointVertex {
    unsigned int index;  // unique index 0..n
    Point3 point;         // the actual point on the layer
    void* predecessor;   // for recording a route
    boost::default_color_type color = boost::white_color;
    coord_t cost = INF;
    coord_t distance = INF;
};
typedef boost::property<boost::edge_weight_t, coord_t> edge_weight_property_t;
typedef boost::adjacency_list<boost::setS, boost::hash_setS, boost::undirectedS, PointVertex, edge_weight_property_t> routing_graph_t;
typedef boost::graph_traits<routing_graph_t>::vertex_descriptor routing_vertex_t;
typedef boost::graph_traits<routing_graph_t>::edge_descriptor routing_edge_t;
typedef std::unordered_map<Point3, routing_vertex_t> point_index_t;
typedef boost::geometry::index::rtree<Point, boost::geometry::index::quadratic<16> > spatial_index_t;
//typedef boost::geometry::index::rtree<Point, boost::geometry::index::rstar<4> > spatial_index_t;

typedef std::map<routing_edge_t, std::pair<Polyline, Polyline>> InterlayerOverlaps;
typedef std::map<coord_t, const ExPolygonCollection*> ExpolygonsMap;
typedef std::map<coord_t, Polylines> PolylinesMap;
typedef std::map<coord_t, spatial_index_t> RtreeMap;

class ElectronicRoutingGraph {
public:
    ElectronicRoutingGraph();
    bool add_vertex(const Point3& p, routing_vertex_t* vertex);
    bool add_vertex(const Point3& p);
    bool add_edge(const Line3& l, routing_edge_t* edge, const double& edge_weight_factor);
    void add_polygon(const Polygon& p, const double& weight_factor, const coord_t print_z);
    bool nearest_point(const Point3& dest, Point3* point);
    bool points_in_boxrange(const Point3& dest, const coord_t range, Points* points);
    void append_z_position(coord_t z);
    bool astar_route(const Point3& start_p, const Point3& goal_p, PolylinesMap* result, const double routing_explore_weight_factor, const double routing_interlayer_factor, const double grid_step_size, const double layer_overlap, const double overlap_tolerance, const double astar_factor);
    void fill_svg(SVG* svg, const coord_t z, const ExPolygonCollection& s = ExPolygonCollection(), const routing_vertex_t& current_v = NULL) const;
    void write_svg(const std::string filename, const coord_t z) const;

    InterlayerOverlaps interlayer_overlaps;
    ExpolygonsMap infill_surfaces_map;  // pointer to infill area polygons for vertex generation in A*
    ExpolygonsMap infill_wire_surfaces_map;  // pointer to infill area polygons including perimeter wires to check whether vertices are crossing a hole
    ExpolygonsMap slices_surfaces_map;  // pointer to slices area polygons, including perimeters for vertex generation in A*

private:
    routing_graph_t graph; // actual boost graph
    boost::property_map<routing_graph_t, Point3 PointVertex::*>::type vertex_index; // Index Vertex->Point
    point_index_t point_index; // Index Point->Vertex

    // boost RTree datastructure for fast range / nearest point access of graph points
    RtreeMap rtree_index; // spatial index

    std::vector<coord_t> z_positions;
};



// Euclidian distance heuristic for routing graph
template <class Graph, class CostType, class LocMap, class PredMap>
class distance_heuristic : public boost::astar_heuristic<Graph, CostType>
{
public:
    typedef typename boost::graph_traits<Graph>::vertex_descriptor Vertex;
    distance_heuristic(LocMap l,
            PredMap p,
            Vertex goal,
            Vertex start,
            const double astar_factor,
            const coord_t step_distance,
            const std::vector<coord_t> &z_positions,
            const double routing_interlayer_factor)
        : m_location(l), m_pred(p), m_start(start), m_goal(goal), astar_factor(astar_factor), step_distance(step_distance), z_positions(z_positions), routing_interlayer_factor(routing_interlayer_factor) {}
    CostType operator()(Vertex u)
    {
        CostType dx = m_location[m_goal].x - m_location[u].x;
        CostType dy = m_location[m_goal].y - m_location[u].y;
        CostType dz = std::abs(m_location[m_goal].z - m_location[u].z);

        // z cost is much higher due to interlayer-factor, so we compensate for this factor
        coord_t min_z = std::min(m_location[u].z, m_location[m_goal].z);
        coord_t max_z = std::max(m_location[u].z, m_location[m_goal].z);
        unsigned int z_hops = 0;

        // how many remaining layer-changes do we expect?
        for (std::vector<coord_t>::const_iterator z = this->z_positions.begin(); z != this->z_positions.end(); ++z) {
            if(*z > min_z && *z <= max_z) {
                z_hops++;
            }
            if(*z > max_z) break;
        }
        CostType const_dz = z_hops * scale_(routing_interlayer_factor);

        // slightly prefer z-positions equally distributed along the x/y path
        double z_corr = 0.0;
        double z_corr_neg = 0.0;
        CostType dz_full = std::abs(m_location[m_goal].z - m_location[m_start].z);
        CostType dxy = ::sqrt(dx * dx + dy * dy);
        if(dz_full > 0) {
            CostType dx_full = m_location[m_goal].x - m_location[m_start].x;
            CostType dy_full = m_location[m_goal].y - m_location[m_start].y;
            CostType dxy_full = ::sqrt(dx_full * dx_full + dy_full * dy_full);
            if(dxy_full > 0) {
                double xy_ratio = dxy / dxy_full;
                double z_ratio = std::min((1.0 * dz / dz_full), 1.0); // limit if dz_full is small
                double xyz_ratio = xy_ratio - z_ratio;
                //std::cout << "xy_ratio: " << xy_ratio << " z_ratio: " << z_ratio << " xyz_ratio: " << xyz_ratio << std::endl;
                z_corr = std::abs(1.0 * xyz_ratio * (10*step_distance));// + scale_(routing_interlayer_factor)));
            }
        }


        // punish strong turns
        CostType angle_corr = 0;
        if(m_pred[u] != u && m_pred[m_pred[u]] != m_pred[u]) { // do we have at least 3 points?

            Point a = m_location[m_pred[m_pred[u]]];
            Point b = m_location[m_pred[u]];
            Point c = m_location[u];

            Vector ab = a.vector_to(b);
            Vector bc = b.vector_to(c);
            double dot_product = ab.x*bc.x + ab.y*bc.y;

            double angle = acos(dot_product / (a.distance_to(b) * b.distance_to(c)));
            // catch instabilities with angle ~ 0
            if(isnan(angle)) angle = 0.0;
            // slightly nudge high angles
            if(angle > 0.79 && angle <= 1.57) { // >45° - <90°
                angle_corr = 1*step_distance;
            }
            if(angle > 1.57 && angle <= 2.35) { // >= 90°
                angle_corr = 10*step_distance;
            }
            if(angle > 2.35 || Line(a, b).contains(c)) { // >= 135°
                angle_corr = 100*step_distance;
            }
        }

        //CostType result = (::sqrt(dx * dx + dy * dy + dz * dz)+const_dz+z_corr)*astar_factor;
        CostType result = (::sqrt(dx * dx + dy * dy + dz * dz)+z_corr+const_dz+angle_corr)*astar_factor;
        //CostType result = (dxy+dz+const_dz+z_corr+angle_corr)*astar_factor;
        //std::cout << "final distance: " << result << std::endl << std::endl;

        return result;
    }
private:
    LocMap m_location;
    PredMap m_pred;
    Vertex m_start;
    Vertex m_goal;
    const double astar_factor;
    const coord_t step_distance;
    const std::vector<coord_t> z_positions;
    const double routing_interlayer_factor;
};

// Visitor that terminates when we find the goal
struct found_goal {}; // exception for termination
template <class BoostGraph, class Vertex, class Edge, class Graph, class Surfaces>
class astar_visitor : public boost::default_astar_visitor
{
public:
    astar_visitor(Vertex goal,
            Graph* graph,
            Surfaces slices_surfaces,
            Surfaces infill_wire_surfaces,
            Surfaces infill_surfaces,
            const std::vector<coord_t> &z_positions,
            InterlayerOverlaps *interlayer_overlaps,
            const coord_t step_distance,
            Slic3r::point_index_t* point_index,
            const double routing_explore_weight_factor,
            const double routing_interlayer_factor,
            const double layer_overlap,
            const double overlap_tolerance)
    : m_goal(goal), graph(graph), slices_surfaces(slices_surfaces), infill_wire_surfaces(infill_wire_surfaces), infill_surfaces(infill_surfaces), z_positions(z_positions), interlayer_overlaps(interlayer_overlaps), step_distance(step_distance), point_index(point_index), routing_explore_weight_factor(routing_explore_weight_factor), routing_interlayer_factor(routing_interlayer_factor), layer_overlap(layer_overlap), overlap_tolerance(overlap_tolerance), debug(false), debug_trigger(true), debug_100(false), iteration(0) {}

    void black_target(Edge e, BoostGraph const& g) {
 /*       std::cout << "REOPEN VERTEX!!! " << e << std::endl;
        std::cout << "a: " << g[boost::source(e, g)].point << std::endl;
        std::cout << "b: " << g[boost::target(e, g)].point << std::endl;

        auto predmap = boost::get(&PointVertex::predecessor, g);
        std::cout << "pred_a: " << predmap[boost::source(e, g)] << std::endl;
        std::cout << "pred_b: " << predmap[boost::target(e, g)] << std::endl;

        BoundingBox bb = infill_surfaces[g[boost::target(e, g)].point.z]->convex_hull().bounding_box();
        bb.offset(scale_(5));
        SVG svg_graph_source("reopen_vertex_source.svg", bb);
        SVG svg_graph_target("reopen_vertex_target.svg", bb);
        this->graph->fill_svg(&svg_graph_source, g[boost::target(e, g)].point.z, *infill_surfaces[g[boost::target(e, g)].point.z], boost::source(e, g));
        this->graph->fill_svg(&svg_graph_target, g[boost::target(e, g)].point.z, *infill_surfaces[g[boost::target(e, g)].point.z], boost::target(e, g));
        Line l(g[boost::source(e, g)].point, g[boost::target(e, g)].point);
        svg_graph_source.draw(l, "red", scale_(0.03));
        svg_graph_target.draw(l, "red", scale_(0.03));
        svg_graph_source.Close();
        svg_graph_target.Close();
        char ch;
        std::cin >> ch;*/
    }

    void examine_vertex(Vertex u, BoostGraph const& g) {
        typedef typename boost::graph_traits<BoostGraph>::vertex_iterator vertex_iterator;
        BoostGraph& g_i = const_cast<BoostGraph&>(g);

        coord_t print_z = g[u].point.z;

        this->iteration++;
        if((debug && !debug_100) || (debug && debug_100 && this->iteration > 100) || (u == m_goal)) {
            std::cout << "explore factor: " << this->routing_explore_weight_factor << " interlayer factor: " << this->routing_interlayer_factor << " layer_overlap: " << this->layer_overlap << std::endl;

            // debug output graph
            std::ostringstream ss;
            ss << "graph_visitor_";
            ss << print_z;
            ss << ".svg";
            std::string filename = ss.str();
            BoundingBox bb = BoundingBox(Point(0, 0), Point(50000, 500000));
            if(((Points)*(infill_surfaces[print_z])).size() > 3) {
                //BoundingBox bb = infill_surfaces[print_z]->convex_hull().bounding_box();
            }
            bb.offset(scale_(5));
            SVG svg_graph(filename.c_str());
            //this->graph->fill_svg(&svg_graph, print_z, *infill_wire_surfaces[print_z], u);
            this->graph->fill_svg(&svg_graph, print_z, *slices_surfaces[print_z], u);
            //svg_graph.draw(*infill_surfaces[print_z], "black");
            svg_graph.Close();

            char ch;
            char d = 'd';
            char c = 'c';
            char h = 'h';
            if(debug && debug_trigger) {
                std::cin >> ch;
            }

            if(ch == c) debug_trigger = false;
            if(ch == d) debug = false;
            if(ch == h) {
                std::cout << "activating iterative debug output every 100 iterations" << std::endl;
                debug_100 = true;
            }
            this->iteration = 0;
        }

        if(u == m_goal) {
        /*    for(const coord_t &z : this->z_positions) {
                std::ostringstream ss;
                ss << "final_graph" << z << ".svg";
                std::string filename = ss.str();
                SVG svg_graph(filename.c_str());
                this->graph->fill_svg(&svg_graph, z, *slices_surfaces[z], u);
                //this->graph->fill_svg(&svg_graph, z, *infill_surfaces[z]);
                svg_graph.Close();
            }
            */

            throw found_goal();
        }

        // are previous and next layer available? at which height?
        coord_t prev_z = -1;
        coord_t next_z = -1;
        for (std::vector<coord_t>::const_iterator z = this->z_positions.begin(); z != this->z_positions.end(); ++z) {
            if(*z == print_z) {
                if(z != this->z_positions.end()-1) {
                    ++z;
                    next_z = *z;
                }
                break;
            }
            prev_z = *z;
        }

        Point3 p = g[u].point;

        // generate infill grid around current vertex and connections to adjacent existing vertices
        this->make_grid_connections(p, g_i);

        // do the same for prev / next layer
        if(prev_z > 0) {
            Point3 pp((Point)p, prev_z);
            this->make_grid_connections(pp, g_i);
        }
        if(next_z > 0) {
            Point3 pp((Point)p, next_z);
            this->make_grid_connections(pp, g_i);
        }
        //this->graph->write_svg("next_z_infill.svg", next_z);


        // traverse back (for overlap) and check for matching points in the neighbor-layers
        boost::property_map<routing_graph_t, boost::edge_weight_t>::const_type EdgeWeightMap = boost::get(boost::edge_weight_t(), g);
        auto predmap = boost::get(&PointVertex::predecessor, g);

        coord_t length = 0;
        double upper_weight = 0;
        double lower_weight = 0;
        Point3 last_point = g[u].point;
        Vertex last_vertex = u;
        Polyline this_trace, upper_trace, lower_trace;
        bool traverse_upper = (next_z > 0);
        bool traverse_lower = (prev_z >= 0);
        for(routing_vertex_t v = u;; v = predmap[v]) {
            if(predmap[v] == v || g[v].point.z != print_z)
                break;

            length += g[v].point.distance_to(last_point);
            if(length > (this->step_distance + this->layer_overlap)) {
                break;
            }

            double w = 0;
            if(v != u) { // only from the second point
                routing_edge_t edge = boost::edge(last_vertex,v,g).first;
                w = boost::get(EdgeWeightMap, edge);
            }

            // matching existing points????
            if(traverse_upper) {
                Point3 p(((Point)g[v].point), next_z);
                Point3 nearest;
                if(this->graph->nearest_point(p, &nearest)) {
                    if(p.distance_to(nearest) <= overlap_tolerance) {
                        upper_trace.append((Point)nearest);
                        upper_weight += w;
                    }else{
                        traverse_upper = false;
                    }
                }else{
                    traverse_upper = false;
                }
            }
            if(traverse_lower) {
                Point3 p(((Point)g[v].point), prev_z);
                Point3 nearest;
                if(this->graph->nearest_point(p, &nearest)) {
                    if(p.distance_to(nearest) <= overlap_tolerance) {
                        lower_trace.append((Point)nearest);
                        // we should consider computing also the lower weight to check which one is higher...!
                        lower_weight += w;
                    }else{
                        traverse_lower = false;
                    }
                }else{
                    traverse_lower = false;
                }
            }

            // can we generate new points inside infill???
            {
                //
            }

            this_trace.append(g[v].point);

            // add upper edge connections
            if(traverse_upper && upper_trace.length() >= this->layer_overlap) {
                Point3 a(this_trace.last_point(), print_z);
                //Point3 b_upper((Point)g[u].point, next_z);
                Point3 b_upper(upper_trace.first_point(), next_z);

                //coord_t distance = upper_trace.length() * this->routing_interlayer_factor;
                coord_t distance = upper_weight + scale_(this->routing_interlayer_factor);


                // we directly add the edge to get the length correct. (not just point-to point distance)
                std::pair<routing_edge_t, bool> existing_edge = boost::edge(point_index->at(a), point_index->at(b_upper), g);
                if(!existing_edge.second) {
                    routing_edge_t edge = boost::add_edge(point_index->at(a), point_index->at(b_upper), distance, g_i).first;
                    (*this->interlayer_overlaps)[edge] = std::make_pair(this_trace, upper_trace);

                    g_i[point_index->at(a)].cost = 0;
                }
                traverse_upper = false;
            }
            // add lower edge connections
            if(traverse_lower && lower_trace.length() >= this->layer_overlap) {
                Point3 a(this_trace.last_point(), print_z);
                //Point3 b_lower((Point)g[u].point, prev_z);
                Point3 b_lower(lower_trace.first_point(), prev_z);

                //coord_t distance = lower_trace.length() * this->routing_interlayer_factor;
                coord_t distance = lower_weight + scale_(this->routing_interlayer_factor);

                // we directly add the edge to get the length correct. (not just point-to point distance)
                std::pair<routing_edge_t, bool> existing_edge = boost::edge(point_index->at(a), point_index->at(b_lower), g);
                if(!existing_edge.second) {
                    routing_edge_t edge = boost::add_edge(point_index->at(a), point_index->at(b_lower), distance, g_i).first;
                    (*this->interlayer_overlaps)[edge] = std::make_pair(lower_trace, this_trace);

                    g_i[point_index->at(a)].cost = 0;
                }

                traverse_lower = false;
            }

            last_point = g[v].point;
            last_vertex = v;
        }
    }

    Point3s generate_grid_points(const Point3& p) {
        Point3s result;
        Point3 p_g = p;
        p_g.translate(step_distance/2, step_distance/2, 0);

        // generate grid candidates
        Point spacing(step_distance, step_distance);
        Point base(0, 0);
        result.push_back(p_g);
        result.back().translate(step_distance, 0, 0);
        result.back().align_to_grid(spacing, base);
        if(!infill_surfaces[p.z]->contains_b(result.back())) result.pop_back();
        result.push_back(p_g);
        result.back().translate(step_distance, -step_distance, 0);
        result.back().align_to_grid(spacing, base);
        if(!infill_surfaces[p.z]->contains_b(result.back())) result.pop_back();
        result.push_back(p_g);
        result.back().translate(0, -step_distance, 0);
        result.back().align_to_grid(spacing, base);
        if(!infill_surfaces[p.z]->contains_b(result.back())) result.pop_back();
        result.push_back(p_g);
        result.back().translate(-step_distance, -step_distance, 0);
        result.back().align_to_grid(spacing, base);
        if(!infill_surfaces[p.z]->contains_b(result.back())) result.pop_back();
        result.push_back(p_g);
        result.back().translate(-step_distance, 0, 0);
        result.back().align_to_grid(spacing, base);
        if(!infill_surfaces[p.z]->contains_b(result.back())) result.pop_back();
        result.push_back(p_g);
        result.back().translate(-step_distance, step_distance, 0);
        result.back().align_to_grid(spacing, base);
        if(!infill_surfaces[p.z]->contains_b(result.back())) result.pop_back();
        result.push_back(p_g);
        result.back().translate(0, step_distance, 0);
        result.back().align_to_grid(spacing, base);
        if(!infill_surfaces[p.z]->contains_b(result.back())) result.pop_back();
        result.push_back(p_g);
        result.back().translate(step_distance, step_distance, 0);
        result.back().align_to_grid(spacing, base);
        if(!infill_surfaces[p.z]->contains_b(result.back())) result.pop_back();

        return result;
    }

    void make_grid_connections(const Point3& p, BoostGraph& g) {
        Point3 nearest = p;
        // is there already a matching point on this layer?
        if(this->graph->nearest_point(p, &nearest)) {
            if(p.distance_to(nearest) > overlap_tolerance) {
                // no, stay with the original point
                nearest = p;
            }
        }

        // is this point inside material (including perimeters)?
        if(this->slices_surfaces[p.z]->contains_b(nearest)) {
        //if(this->infill_surfaces[p.z]->contains_b(nearest)) {
            // add p to graph, gets vertex handle if p already exists
            Vertex u;
            this->graph->add_vertex(nearest, &u);


            // add vertices in 45° steps if they are inside the infill region
            Point3s pts;
            if(this->infill_surfaces[p.z]->contains_b(nearest)) { // is this point inside infill?
                pts = generate_grid_points(nearest);
            }

            // check existing points within grid distance
            Points near_pts;

            // dummy variables
            Line line(nearest, nearest);

            if(this->graph->points_in_boxrange(nearest, this->step_distance, &near_pts)) {
                for(Point &pt : near_pts) {
                    if(this->slices_surfaces[p.z]->contains_b(pt)) { // skip points outside of material
                        // and test if this edge crosses a boundary. If yes: skip.
                        // for performance reasons we just do a simplified test an check whether the mid-point is inside polygon
                        line.b = pt;
                        // TODO: this should be checked against plastic infill region, not wire (inflated slices) infill?
                        // to avoid shortcuts at angles
                        if(this->infill_wire_surfaces[p.z]->contains(line.midpoint())) {
                        //if(this->infill_surfaces[p.z]->contains(line.midpoint())) {
                            pts.push_back(Point3(pt, p.z));
                        }
                    }
                }
            }

            // insert vertices and edges to graph
            for(Point3 &pt : pts) {
                Vertex v;
                this->graph->add_vertex(pt, &v);
                coord_t distance = p.distance_to(pt) * this->routing_explore_weight_factor;
                // avoid self-loops
                if(u != v) {
                    /*std::pair<routing_edge_t, bool> old_edge = boost::edge(u,v,g);
                    std::pair<routing_edge_t, bool> new_edge = boost::add_edge(u, v, distance, g);

                    if(old_edge.second) {
                        if(old_edge.first != new_edge.first) {
                            std::cout << "Edge " << old_edge.first << "already exists and is overwritten by new edge " << new_edge.first << std::endl;
                        }
                    }
                    if(!new_edge.second) {
                        if(old_edge.second) {
                            std::cout << "new edge could not be inserted!!! old edge is: " << old_edge.first << std::endl;
                        }else{
                            std::cout << "new edge could not be inserted!!! NO OLD EDGE!!!" << std::endl;
                        }
                    }*/
                    boost::add_edge(u, v, distance, g);
                }
            }
        }
    }

private:
    Vertex m_goal;
    Graph* graph;
    Surfaces slices_surfaces;
    Surfaces infill_wire_surfaces;
    Surfaces infill_surfaces;
    const std::vector<coord_t> z_positions;
    InterlayerOverlaps *interlayer_overlaps;
    const coord_t step_distance;
    point_index_t* point_index;
    const double routing_explore_weight_factor;
    const double routing_interlayer_factor;
    const double layer_overlap;
    const double overlap_tolerance;
    bool debug;
    bool debug_trigger;
    bool debug_100;
    unsigned int iteration;
};
}

#endif
