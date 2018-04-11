#ifndef slic3r_ElectronicWireGenerator_hpp_
#define slic3r_ElectronicWireGenerator_hpp_

#include "libslic3r.h"
#include "Layer.hpp"
#include "Print.hpp"
#include "Polyline.hpp"
#include "Polygon.hpp"
#include "ExPolygonCollection.hpp"

#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/graph_traits.hpp>
#include <boost/graph/graphviz.hpp>
#include <boost/graph/astar_search.hpp>

namespace Slic3r {


class ElectronicWireGenerator;
typedef boost::property<boost::edge_weight_t, coord_t> edge_weight_property_t;
typedef boost::property<boost::vertex_index_t, Point, boost::property<boost::vertex_index1_t, int> > point_property_t;
typedef boost::adjacency_list<boost::setS, boost::hash_setS, boost::undirectedS, point_property_t, edge_weight_property_t> routing_graph_t;
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
    bool add_vertex(const Point& p, routing_graph_t* g, point_index_t* index) const;
    bool add_edge(const Line& l, routing_graph_t* g, point_index_t* index, routing_edge_t* edge, const double& edge_weight_factor = 1.0) const;

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
template <class Vertex>
class astar_goal_visitor : public boost::default_astar_visitor
{
public:
    astar_goal_visitor(Vertex goal) : m_goal(goal) {}
    template <class Graph>
    void examine_vertex(Vertex u, Graph& g) {
        if(u == m_goal)
            throw found_goal();
    }
private:
    Vertex m_goal;
};

}

#endif
