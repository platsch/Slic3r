#ifndef slic3r_ElectronicRoutingGraph_hpp_
#define slic3r_ElectronicRoutingGraph_hpp_

#include "Polygon.hpp"
#include  "SVG.hpp"

#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/graph_traits.hpp>
#include <boost/graph/graphviz.hpp>
#include <boost/graph/astar_search.hpp>

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

typedef std::map<routing_edge_t, Polyline> InterlayerOverlaps;
typedef std::map<coord_t, ExPolygonCollection*> ExpolygonsMap;
typedef std::map<coord_t, Polylines> PolylinesMap;
typedef std::map<coord_t, spatial_index_t> RtreeMap;

class ElectronicRoutingGraph {
public:
    ElectronicRoutingGraph();
    bool add_vertex(const Point3& p, routing_vertex_t* vertex);
    bool add_vertex(const Point3& p);
    bool add_edge(const Line3& l, routing_edge_t* edge, const double& edge_weight_factor);
    bool add_polygon(const Polygon& p, const double& weight_factor, const coord_t print_z);
    bool nearest_point(const Point3& dest, Point3* point);
    bool points_in_boxrange(const Point3& dest, const coord_t range, Points* points);
    void append_z_position(coord_t z);
    bool astar_route(const Point3& start_p, const Point3& goal_p, PolylinesMap* result, const double routing_explore_weight_factor, const double routing_interlayer_factor, const double layer_overlap, const double astar_factor);
    void fill_svg(SVG* svg, const coord_t z, const ExPolygonCollection& s = ExPolygonCollection(), const routing_vertex_t& current_v = NULL) const;
    void write_svg(const std::string filename, const coord_t z) const;

    InterlayerOverlaps interlayer_overlaps;
    ExpolygonsMap infill_surfaces_map;  // pointer to infill area polygons for vertex generation in A*

private:
    routing_graph_t graph; // actual boost graph
    boost::property_map<routing_graph_t, Point3 PointVertex::*>::type vertex_index; // Index Vertex->Point
    point_index_t point_index; // Index Point->Vertex

    // boost RTree datastructure for fast range / nearest point access of graph points
    RtreeMap rtree_index; // spatial index

    std::vector<coord_t> z_positions;
};



// Euclidean distance heuristic for routing graph
template <class Graph, class CostType, class LocMap>
class distance_heuristic : public boost::astar_heuristic<Graph, CostType>
{
public:
    typedef typename boost::graph_traits<Graph>::vertex_descriptor Vertex;
    distance_heuristic(LocMap l, Vertex goal, const double astar_factor)
        : m_location(l), m_goal(goal), astar_factor(astar_factor) {}
    CostType operator()(Vertex u)
    {
        CostType dx = m_location[m_goal].x - m_location[u].x;
        CostType dy = m_location[m_goal].y - m_location[u].y;
        CostType dz = m_location[m_goal].z - m_location[u].z;
        return ::sqrt(dx * dx + dy * dy + dz * dz)*astar_factor;
    }
private:
    LocMap m_location;
    Vertex m_goal;
    double astar_factor;
};

// Visitor that terminates when we find the goal
struct found_goal {}; // exception for termination
template <class BoostGraph, class Vertex, class Edge, class Graph, class Surfaces>
class astar_visitor : public boost::default_astar_visitor
{
public:
    astar_visitor(Vertex goal,
            Graph* graph,
            Surfaces surfaces,
            const std::vector<coord_t> &z_positions,
            InterlayerOverlaps *interlayer_overlaps,
            const coord_t step_distance,
            Slic3r::point_index_t* point_index,
            const double routing_explore_weight_factor,
            const double routing_interlayer_factor,
            const double layer_overlap)
    : m_goal(goal), graph(graph), surfaces(surfaces), z_positions(z_positions), interlayer_overlaps(interlayer_overlaps), step_distance(step_distance), point_index(point_index), routing_explore_weight_factor(routing_explore_weight_factor), routing_interlayer_factor(routing_interlayer_factor), layer_overlap(layer_overlap), debug(true), debug_trigger(true) {}

    void black_target(Edge e, BoostGraph const& g) {
        std::cout << "REOPEN VERTEX!!!" << std::endl;
        std::cout << "a: " << g[boost::source(e, g)].point << std::endl;
        std::cout << "b: " << g[boost::target(e, g)].point << std::endl;
    }

    void examine_vertex(Vertex u, BoostGraph const& g) {
        typedef typename boost::graph_traits<BoostGraph>::vertex_iterator vertex_iterator;
        BoostGraph& g_i = const_cast<BoostGraph&>(g);

        coord_t print_z = g[u].point.z;
        if(debug || (u == m_goal)) {
            std::cout << "explore factor: " << this->routing_explore_weight_factor << " interlayer factor: " << this->routing_interlayer_factor << " layer_overlap: " << this->layer_overlap << std::endl;

            // debug output graph
            std::ostringstream ss;
            ss << "graph_visitor_";
            ss << print_z;
            ss << ".svg";
            std::string filename = ss.str();
            BoundingBox bb = surfaces[print_z]->convex_hull().bounding_box();
            bb.offset(scale_(5));
            SVG svg_graph(filename.c_str(), bb);
            this->graph->fill_svg(&svg_graph, print_z, *surfaces[print_z], u);
            svg_graph.Close();

            char ch;
            char d = 'd';
            char c = 'c';
            if(debug && debug_trigger) {
                std::cin >> ch;
            }

            if(ch == c) debug_trigger = false;
            if(ch == d) debug = false;
        }


        if(u == m_goal) {
            throw found_goal();
        }

        // add vertices in 45° steps if they are inside the infill region
        Point vertex_p = (Point)g[u].point;
        Points pts;
        if(surfaces[print_z]->contains_b(vertex_p)) { // is this point inside infill?
            // generate grid candidates
            Point spacing(step_distance, step_distance);
            Point base(0, 0);
            pts.push_back(Point(vertex_p));
            pts.back().translate(step_distance, 0);
            pts.back().align_to_grid(spacing, base);
            if(!surfaces[print_z]->contains_b(pts.back())) pts.pop_back();
            pts.push_back(Point(vertex_p));
            pts.back().translate(step_distance, -step_distance);
            pts.back().align_to_grid(spacing, base);
            if(!surfaces[print_z]->contains_b(pts.back())) pts.pop_back();
            pts.push_back(Point(vertex_p));
            pts.back().translate(0, -step_distance);
            pts.back().align_to_grid(spacing, base);
            if(!surfaces[print_z]->contains_b(pts.back())) pts.pop_back();
            pts.push_back(Point(vertex_p));
            pts.back().translate(-step_distance, -step_distance);
            pts.back().align_to_grid(spacing, base);
            if(!surfaces[print_z]->contains_b(pts.back())) pts.pop_back();
            pts.push_back(Point(vertex_p));
            pts.back().translate(-step_distance, 0);
            pts.back().align_to_grid(spacing, base);
            if(!surfaces[print_z]->contains_b(pts.back())) pts.pop_back();
            pts.push_back(Point(vertex_p));
            pts.back().translate(-step_distance, step_distance);
            pts.back().align_to_grid(spacing, base);
            if(!surfaces[print_z]->contains_b(pts.back())) pts.pop_back();
            pts.push_back(Point(vertex_p));
            pts.back().translate(0, step_distance);
            pts.back().align_to_grid(spacing, base);
            if(!surfaces[print_z]->contains_b(pts.back())) pts.pop_back();
            pts.push_back(Point(vertex_p));
            pts.back().translate(step_distance, step_distance);
            pts.back().align_to_grid(spacing, base);
            if(!surfaces[print_z]->contains_b(pts.back())) pts.pop_back();
        }

        // check existing points within grid distance
        Points near_pts;
        if(this->graph->points_in_boxrange(g[u].point, step_distance, &near_pts)) {
            pts.insert(pts.end(), near_pts.begin(), near_pts.end());
        }

        // insert vertices and edges to graph
        for(auto &p : pts) {
            //if(surfaces[print_z]->contains_b(p)) { // is this point inside infill?
            Vertex v;
            this->graph->add_vertex(Point3(p, print_z), &v);
            coord_t distance = vertex_p.distance_to(p) * routing_explore_weight_factor;
            // avoid self-loops
            if(u != v) {
                boost::add_edge(u, v, distance, g_i);
            }
            //}
        }

        // traverse back (for overlap) and check for matching points in the neighbor-layers
        boost::property_map<routing_graph_t, boost::edge_weight_t>::const_type EdgeWeightMap = boost::get(boost::edge_weight_t(), g);
        auto predmap = boost::get(&PointVertex::predecessor, g);
        coord_t prev_z = -1;
        coord_t next_z = -1;
        for (std::vector<coord_t>::const_iterator z = this->z_positions.begin(); z != this->z_positions.end(); ++z) {
            if(*z == print_z) {
                if(z != this->z_positions.end()) {
                    ++z;
                    next_z = *z;
                }
                break;
            }
            prev_z = *z;
        }
        coord_t length = 0;
        double upper_weight = 0;
        double lower_weight = 0;
        Point3 last_point = g[u].point;
        Vertex last_vertex = u;
        Polyline upper_trace, lower_trace;
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
                try {
                    Point3 p(((Point)g[v].point), next_z);
                    routing_vertex_t vertex = point_index->at(p); // unordered_map::at throws an out-of-range
                    upper_trace.append((Point)(g[vertex].point));
                    upper_weight += w;
                } catch (const std::out_of_range& oor) {
                    traverse_upper = false;
                }
            }
            if(traverse_lower) {
                try {
                    Point3 p(((Point)g[v].point), prev_z);
                    routing_vertex_t vertex = point_index->at(p); // unordered_map::at throws an out-of-range
                    lower_trace.append((Point)(g[vertex].point));
                    lower_weight += w;
                } catch (const std::out_of_range& oor) {
                    traverse_lower = false;
                }
            }

            // can we generate new points inside infill???
            {
                //
            }


            // add upper edge connections
            if(traverse_upper && upper_trace.length() >= this->layer_overlap) {
                Point3 a(upper_trace.last_point(), print_z);
                Point3 b_upper((Point)g[u].point, next_z);

                //coord_t distance = upper_trace.length() * this->routing_interlayer_factor;
                coord_t distance = upper_weight + scale_(this->routing_interlayer_factor);
                // we directly add the edge to get the length correct. (not just point-to point distance)
                routing_edge_t edge = boost::add_edge(point_index->at(a), point_index->at(b_upper), distance, g_i).first;
                (*this->interlayer_overlaps)[edge] = upper_trace;
                //std::cout << "upper_trace weight: " << upper_weight << " vs length: " << upper_trace.length() << " vs factor: " << scale_(this->routing_interlayer_factor) << std::endl;
            }
            // add lower edge connections
            if(traverse_lower && lower_trace.length() >= this->layer_overlap) {
                Point3 a(lower_trace.last_point(), print_z);
                Point3 b_lower((Point)g[u].point, prev_z);

                //coord_t distance = lower_trace.length() * this->routing_interlayer_factor;
                coord_t distance = lower_weight + scale_(this->routing_interlayer_factor);
                // we directly add the edge to get the length correct. (not just point-to point distance)
                routing_edge_t edge = boost::add_edge(point_index->at(a), point_index->at(b_lower), distance, g_i).first;
                (*this->interlayer_overlaps)[edge] = lower_trace;
            }

            last_point = g[v].point;
            last_vertex = v;
        }
    }
private:
    Vertex m_goal;
    Graph* graph;
    Surfaces surfaces;
    const std::vector<coord_t> z_positions;
    InterlayerOverlaps *interlayer_overlaps;
    const coord_t step_distance;
    point_index_t* point_index;
    const double routing_explore_weight_factor;
    const double routing_interlayer_factor;
    const double layer_overlap;
    bool debug;
    bool debug_trigger;
};
}

#endif
