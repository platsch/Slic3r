#ifndef slic3r_ElectronicWireGenerator_hpp_
#define slic3r_ElectronicWireGenerator_hpp_

#include "libslic3r.h"
#include "Layer.hpp"
#include "Print.hpp"
#include "Polyline.hpp"
#include "Polygon.hpp"
#include "ExPolygonCollection.hpp"
#include  "SVG.hpp"

#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/graph_traits.hpp>
#include <boost/graph/graphviz.hpp>
#include <boost/graph/astar_search.hpp>

namespace Slic3r {


class ElectronicWireGenerator;

constexpr coord_t INF = std::numeric_limits<coord_t>::max();

// Define boost graph types
struct PointVertex {
    unsigned int index;  // unique index 0..n
    Point point;         // the actual point on the layer
    void* predecessor;   // for recording a route
    boost::default_color_type color = boost::white_color;
    coord_t cost = INF;
    coord_t distance = INF;
};
typedef boost::property<boost::edge_weight_t, coord_t> edge_weight_property_t;
typedef boost::adjacency_list<boost::setS, boost::hash_setS, boost::undirectedS, PointVertex, edge_weight_property_t> routing_graph_t;
typedef boost::graph_traits<routing_graph_t>::vertex_descriptor routing_vertex_t;
typedef boost::graph_traits<routing_graph_t>::edge_descriptor routing_edge_t;
typedef std::unordered_map<Point, routing_vertex_t> point_index_t;

struct WireSegment {
    Line line;
    std::vector<std::pair<Line, double> > connecting_lines;
    std::set<routing_edge_t> edges;
};

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
    void polygon_to_graph(const Polygon& p, routing_graph_t* g, point_index_t* index, const double& weight_factor = 1.0) const;

    // Inputs:
    Layer* layer;             ///< pointer to layer object
    Polylines unrouted_wires; ///< copy of original path collection,
                              /// will be deleted during process()
    Polylines routed_wires;   ///< intermediate state after contour following
    double extrusion_width;
    double extrusion_overlap;
    double first_extrusion_overlap;
    double overlap_min_extrusion_length;
    double conductive_wire_channel_width;
    const ExPolygonCollection* slices;
};


// helper functions for graph operations
inline bool add_point_vertex(const Point& p, routing_graph_t& g, point_index_t& index, routing_vertex_t* vertex)
{
    bool result = false;
    if(index.find(p) == index.end()) { // is this point already in the graph?
        (*vertex) = boost::add_vertex(g);
        (g)[(*vertex)].index = boost::num_vertices(g)-1;
        (g)[(*vertex)].point = p;
        (g)[(*vertex)].predecessor = (*vertex);
        index[p] = (*vertex); // add vertex to reverse lookup table
        result = true;
    }else{
        // return existing vertex
        *vertex = index[p];
    }
    return result;
}

inline bool add_point_vertex(const Point& p, routing_graph_t& g, point_index_t& index)
{
    routing_vertex_t v;
    return Slic3r::add_point_vertex(p, g, index, &v);
}

inline bool add_point_edge(const Line& l, routing_graph_t& g, point_index_t& index, routing_edge_t* edge, const double& edge_weight_factor)
{
    bool result = true;
    try {
        routing_vertex_t vertex_a = index.at(l.a); // unordered_map::at throws an out-of-range
        routing_vertex_t vertex_b = index.at(l.b); // unordered_map::at throws an out-of-range
        coord_t distance = l.length() * edge_weight_factor;
        *edge = boost::add_edge(vertex_a, vertex_b, distance, g).first;
    } catch (const std::out_of_range& oor) {
        std::cerr << "Unable to find vertex for point while adding an edge: " << oor.what() << '\n';
        result = false;
    }
    return result;
}



// Euclidean distance heuristic for routing graph
template <class Graph, class CostType, class LocMap>
class distance_heuristic : public boost::astar_heuristic<Graph, CostType>
{
public:
    typedef typename boost::graph_traits<Graph>::vertex_descriptor Vertex;
    distance_heuristic(LocMap l, Vertex goal)
        : m_location(l), m_goal(goal) {}
    CostType operator()(Vertex u)
    {
        CostType dx = m_location[m_goal].x - m_location[u].x;
        CostType dy = m_location[m_goal].y - m_location[u].y;
        return ::sqrt(dx * dx + dy * dy);
    }
private:
    LocMap m_location;
    Vertex m_goal;
};


// Visitor that terminates when we find the goal
struct found_goal {}; // exception for termination
template <class Graph, class Vertex, class Surfaces>
class astar_visitor : public boost::default_astar_visitor
{
public:
    astar_visitor(Vertex goal, const Surfaces* surfaces, const coord_t step_distance, point_index_t* point_index, double edge_weight_factor)
    : m_goal(goal), surfaces(surfaces), step_distance(step_distance), point_index(point_index), edge_weight_factor(edge_weight_factor), debug(false) {}
    void examine_vertex(Vertex u, Graph const& g) {
        //std::cout << "examine vertex " << u << " vs goal " << m_goal <<  std::endl;

        Graph& g_i = const_cast<Graph&>(g);
        if(debug) {
            // debug output graph
            SVG svg_graph("graph_visitor", BoundingBox(Point(0, 0), Point(scale_(50), scale_(50))));
            svg_graph.draw(*surfaces, "black");
            boost::graph_traits<routing_graph_t>::edge_iterator ei, ei_end;
            for (boost::tie(ei, ei_end) = edges(g); ei != ei_end; ++ei) {
                Line l = Line(g[boost::source(*ei, g)].point, g[boost::target(*ei, g)].point);
                boost::property_map<routing_graph_t, boost::edge_weight_t>::type EdgeWeightMap = boost::get(boost::edge_weight_t(), g_i);
                coord_t weight = boost::get(EdgeWeightMap, *ei);
                double w = weight / l.length();
                w -=0.99;
                w = w*500;
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
                svg_graph.draw(l,  color, scale_(0.1));
            }

            // draw current route
            auto predmap = boost::get(&PointVertex::predecessor, g);
            Point last_point = g[u].point;
            for(routing_vertex_t v = u;; v = predmap[v]) {
                Line l = Line(last_point, g[v].point);
                svg_graph.draw(l,  "green", scale_(0.07));
                last_point = g[v].point;
                if(predmap[v] == v)
                    break;
            }

            svg_graph.Close();

            char ch;
            char d = 'd';
            std::cin >> ch;

            if(ch == d) debug = false;
        }


        if(u == m_goal) {
            throw found_goal();
        }

        // add vertices in 45° steps if they are inside the infill region
        Point vertex_p = g[u].point;
        if(surfaces->contains_b(vertex_p)) { // is this point inside infill?
            Graph& g_i = const_cast<Graph&>(g);
            Points pts;

            // generate grid candidates
            Point spacing(step_distance, step_distance);
            Point base(base);
            pts.push_back(Point(vertex_p));
            pts.back().translate(step_distance, 0);
            pts.back().align_to_grid(spacing, base);
            pts.push_back(Point(vertex_p));
            pts.back().translate(step_distance, -step_distance);
            pts.back().align_to_grid(spacing, base);
            pts.push_back(Point(vertex_p));
            pts.back().translate(0, -step_distance);
            pts.back().align_to_grid(spacing, base);
            pts.push_back(Point(vertex_p));
            pts.back().translate(-step_distance, -step_distance);
            pts.back().align_to_grid(spacing, base);
            pts.push_back(Point(vertex_p));
            pts.back().translate(-step_distance, 0);
            pts.back().align_to_grid(spacing, base);
            pts.push_back(Point(vertex_p));
            pts.back().translate(-step_distance, step_distance);
            pts.back().align_to_grid(spacing, base);
            pts.push_back(Point(vertex_p));
            pts.back().translate(0, step_distance);
            pts.back().align_to_grid(spacing, base);
            pts.push_back(Point(vertex_p));
            pts.back().translate(step_distance, step_distance);
            pts.back().align_to_grid(spacing, base);

            // check existing points within grid distance
            typedef typename boost::graph_traits<Graph>::vertex_iterator vertex_iterator;
            vertex_iterator i, iend;
            for (boost::tie(i, iend) = boost::vertices(g); i != iend; ++i) {
                if(vertex_p.distance_to(g[*i].point) < step_distance) {
                    //std::cout << "distance from vertex " << vertex_p << " to point " << g[*i].point << " is " << vertex_p.distance_to(g[*i].point) << std::endl;
                    pts.push_back(g[*i].point);
                }
            }

            for(auto &p : pts) {
                if(surfaces->contains_b(p)) { // is this point inside infill?
                    //std::cout << "point " << p << "is inside infill, adding it!" << std::endl;
                    Vertex v;
                    Slic3r::add_point_vertex(p, g_i, *point_index, &v);
                    coord_t distance = vertex_p.distance_to(p) * edge_weight_factor;
                    boost::add_edge(u, v, distance, g_i);
                }
            }


/*            // debug output graph
            SVG svg_graph("graph_visitor", BoundingBox(Point(0, 0), Point(scale_(50), scale_(50))));
            svg_graph.draw(*surfaces, "black");
            boost::graph_traits<routing_graph_t>::edge_iterator ei, ei_end;
            for (boost::tie(ei, ei_end) = edges(g); ei != ei_end; ++ei) {
                Line l = Line(g[boost::source(*ei, g)].point, g[boost::target(*ei, g)].point);
                boost::property_map<routing_graph_t, boost::edge_weight_t>::type EdgeWeightMap = boost::get(boost::edge_weight_t(), g_i);
                coord_t weight = boost::get(EdgeWeightMap, *ei);
                double w = weight / l.length();
                //std::cout << "weight: " << w << std::endl;
                w -=0.99;
                w = w*500;
                int w_int = std::min((int)w, 255);
                //std::cout << "weight color: " << w_int << std::endl;
                std::ostringstream ss;
                ss << "rgb(" << 100 << "," << w_int << "," << w_int << ")";
                std::string color = ss.str();
                //std::cout << "color: " << color << std::endl;
                svg_graph.draw(l,  color, scale_(0.1));
            }

            // draw current route
            auto predmap = boost::get(&PointVertex::predecessor, g);
            Point last_point = g[u].point;
            for(routing_vertex_t v = u;; v = predmap[v]) {
                //std::cout << "predmap[" << v << "] = " << predmap[v] << std::endl;
                Line l = Line(last_point, g[v].point);
                svg_graph.draw(l,  "green", scale_(0.07));
                last_point = g[v].point;
                if(predmap[v] == v)
                    break;
            }


            svg_graph.draw(vertex_p, "green", scale_(0.5));
            svg_graph.Close();

            if(debug) {
                char ch;
                char d = 'd';
                std::cin >> ch;

                if(ch == d) debug = false;
            }
*/
        }


    }
private:
    Vertex m_goal;
    const Surfaces* surfaces;
    const coord_t step_distance;
    point_index_t* point_index;
    const double edge_weight_factor;
    bool debug;
};

}

#endif
