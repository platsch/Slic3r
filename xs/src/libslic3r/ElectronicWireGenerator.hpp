#ifndef slic3r_ElectronicWireGenerator_hpp_
#define slic3r_ElectronicWireGenerator_hpp_

#include "libslic3r.h"
#include "RubberBand.hpp"
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
class ElectronicWireRouter;
typedef std::vector<ElectronicWireGenerator> ElectronicWireGenerators;
typedef std::vector<ElectronicWireGenerator*> ElectronicWireGeneratorPtrs;

constexpr coord_t INF = std::numeric_limits<coord_t>::max();

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

struct WireSegment {
    Line line;
    std::vector<std::pair<Line, double> > connecting_lines;
    std::set<routing_edge_t> edges;
};
typedef std::vector<WireSegment> WireSegments;
typedef std::map<coord_t, ExPolygonCollection*> ExpolygonsMap;
typedef std::map<routing_edge_t, Polyline> InterlayerOverlaps;

class ElectronicWireGenerator {
public:
    ElectronicWireGenerator(
        /// Input:
        /// pointer to layer object on which wires will be refined
        Layer*                     layer,
        ElectronicWireGenerator*   previous_ewg,
        double                     extrusion_width,
        double                     extrusion_overlap,
        double                     first_extrusion_overlap,
        double                     overlap_min_extrusion_length,
        double                     conductive_wire_channel_width);

    const coordf_t get_print_z() const;
    const coord_t get_scaled_print_z() const;
    const coordf_t get_bottom_z() const;
    const coordf_t get_scaled_bottom_z() const;
    ExPolygonCollections* get_contour_set();
    WireSegments get_wire_segments(Lines& wire, const double routing_perimeter_factor, const double routing_hole_factor);
    void add_routed_wire(Polyline &routed_wire);

    void generate_wires();

private:
    void sort_unrouted_wires();
    void channel_from_wire(Polyline &wire);
    void apply_overlap(Polylines input, Polylines *output);
    coord_t offset_width(const LayerRegion* region, int perimeters) const;
    void _align_to_prev_perimeters();
    //void polygon_to_graph(const Polygon& p, routing_graph_t* g, point_index_t* index, const double& weight_factor = 1.0) const;

    // Inputs:
    Layer* layer;             ///< pointer to layer object
    ElectronicWireGenerator* previous_ewg;  ///< pointer to previous wire generator to align polygons
    Polylines unrouted_wires; ///< copy of original path collection,
                              /// will be deleted during process()
    Polylines routed_wires;   ///< intermediate state after contour following
    double extrusion_width;
    double extrusion_overlap;
    double first_extrusion_overlap;
    double overlap_min_extrusion_length;
    double conductive_wire_channel_width;
    int max_perimeters;
    const ExPolygonCollection* slices;
    ExPolygonCollections deflated_slices;

    friend class ElectronicWireRouter;
};


class ElectronicWireRouter {
public:
    ElectronicWireRouter(
            const double layer_overlap,
            const double routing_astar_factor,
            const double routing_perimeter_factor,
            const double routing_hole_factor,
            const double routing_interlayer_factor,
            const int layer_count);
    void append_wire_generator(ElectronicWireGenerator& ewg);
    ElectronicWireGenerator* last_ewg();
    void route(const RubberBand* rb, const Point3 offset);
    void generate_wires();

private:
    coord_t _map_z_to_layer(coord_t z) const;

    ElectronicWireGenerators ewgs;
    std::map<coord_t, ElectronicWireGenerator*> z_map;
    const double layer_overlap;
    const double routing_astar_factor;
    const double routing_perimeter_factor;
    const double routing_hole_factor;
    const double routing_interlayer_factor;
};

// helper functions for graph operations
inline bool add_point_vertex(const Point3& p, routing_graph_t& g, point_index_t& index, routing_vertex_t* vertex)
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

inline bool add_point_vertex(const Point3& p, routing_graph_t& g, point_index_t& index)
{
    routing_vertex_t v;
    return Slic3r::add_point_vertex(p, g, index, &v);
}

inline bool add_point_edge(const Line3& l, routing_graph_t& g, point_index_t& index, routing_edge_t* edge, const double& edge_weight_factor)
{
    bool result = true;
    try {
        routing_vertex_t vertex_a = index.at(l.a); // unordered_map::at throws an out-of-range
        routing_vertex_t vertex_b = index.at(l.b); // unordered_map::at throws an out-of-range
        coord_t distance = l.length() * edge_weight_factor;
        *edge = boost::add_edge(vertex_a, vertex_b, distance, g).first;
    } catch (const std::out_of_range& oor) {
        std::cerr << "Unable to find vertex for point while adding an edge: " << oor.what() << '\n';
        std::cout << "Line was: " << l << std::endl;
        result = false;
    }
    return result;
}

inline void polygon_to_graph(const Polygon& p, routing_graph_t* g, point_index_t* index, const double& weight_factor, const coord_t print_z)
{
    if(p.is_valid()) {
        routing_vertex_t last_vertex, this_vertex, first_vertex;
        Slic3r::add_point_vertex(Point3(p.points.front(), print_z) , *g, *index, &last_vertex); // add first point
        first_vertex = last_vertex;
        for (Points::const_iterator it = p.points.begin(); it != p.points.end()-1; ++it) {
            // add next vertex
            Slic3r::add_point_vertex(Point3(*(it+1), print_z), *g, *index, &this_vertex);

            // add edge
            coord_t distance = it->distance_to(*(it+1)) * weight_factor;
            // Add factor to prioritize routing with high distance to external perimeter
            boost::add_edge(last_vertex, this_vertex, distance, *g);
            last_vertex = this_vertex;
        }
        // add final edge to close the loop
        coord_t distance = p.points.back().distance_to(p.points.front()) * weight_factor;
        // Add factor to prioritize routing with high distance to external perimeter
        boost::add_edge(last_vertex, first_vertex, distance, *g);
    }
}

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
template <class Graph, class Vertex, class Edge, class Surfaces>
class astar_visitor : public boost::default_astar_visitor
{
public:
    astar_visitor(Vertex goal,
            Surfaces surfaces,
            const std::vector<coord_t> &z_positions,
            InterlayerOverlaps *interlayer_overlaps,
            const coord_t step_distance,
            Slic3r::point_index_t* point_index,
            const double routing_explore_weight_factor,
            const double routing_interlayer_factor,
            const double layer_overlap)
    : m_goal(goal), surfaces(surfaces), z_positions(z_positions), interlayer_overlaps(interlayer_overlaps), step_distance(step_distance), point_index(point_index), routing_explore_weight_factor(routing_explore_weight_factor), routing_interlayer_factor(routing_interlayer_factor), layer_overlap(layer_overlap), debug(true), debug_trigger(true) {}

    void black_target(Edge e, Graph const& g) {
        std::cout << "REOPEN VERTEX!!!" << std::endl;
    }

    void examine_vertex(Vertex u, Graph const& g) {
        //std::cout << "examine vertex " << u << " vs goal " << m_goal <<  std::endl;

        typedef typename boost::graph_traits<Graph>::vertex_iterator vertex_iterator;

        Graph& g_i = const_cast<Graph&>(g);
        coord_t print_z = g[u].point.z;
        if(debug || (u == m_goal)) {
            std::cout << "explore factor: " << this->routing_explore_weight_factor << " interlayer factor: " << this->routing_interlayer_factor << " layer_overlap: " << this->layer_overlap << std::endl;
            // debug output graph
            std::ostringstream ss;
            ss << "graph_visitor_";
            ss << print_z;
            ss << ".svg";
            std::string filename = ss.str();
            SVG svg_graph(filename.c_str());
            svg_graph.draw(*surfaces[print_z], "black");
            boost::graph_traits<routing_graph_t>::edge_iterator ei, ei_end;
            for (boost::tie(ei, ei_end) = edges(g); ei != ei_end; ++ei) {
                if(g[boost::source(*ei, g)].point.z == print_z || g[boost::target(*ei, g)].point.z == print_z) {
                    Line l = Line((Point)g[boost::source(*ei, g)].point, (Point)g[boost::target(*ei, g)].point);
                    boost::property_map<routing_graph_t, boost::edge_weight_t>::type EdgeWeightMap = boost::get(boost::edge_weight_t(), g_i);
                    coord_t weight = boost::get(EdgeWeightMap, *ei);
                    double w = weight / l.length();
                    w = w*20;
                    int w_int = std::min((int)w, 255);
                    std::string color;
                    if(w_int < 0) {
                        color = "green";
                    }else{
                        std::ostringstream ss;
                        ss << "rgb(" << 100 << "," << w_int << "," << w_int << ")";
                        color = ss.str();
                    }
                    // interlayer-connections
                    if(g[boost::source(*ei, g)].point.z != g[boost::target(*ei, g)].point.z) {
                        color = "green";
                    }
                    //std::cout << "color: " << color << std::endl;
                    //if(g[boost::source(*ei, g)].color == boost::white_color) {
                    //    svg_graph.draw(l,  "white", scale_(0.1));
                    //}else{
                    //    svg_graph.draw(l,  "blue", scale_(0.1));
                    //}
                    svg_graph.draw(l, color, scale_(0.1));
                    svg_graph.draw(l.a, "red", scale_(0.1));
                    svg_graph.draw(l.b, "red", scale_(0.1));
                    if(l.length() < 2) {
                        svg_graph.draw(l.a, "red", scale_(0.2));
                        std::cout << "line lenght 0, z_a: " << g[boost::source(*ei, g)].point.z << " z_b: " << g[boost::target(*ei, g)].point.z << std::endl;
                    }
                }
            }


            // draw current route
            auto predmap = boost::get(&PointVertex::predecessor, g);
            Point last_point = (Point)g[u].point;
            for(routing_vertex_t v = u;; v = predmap[v]) {
                Line l = Line(last_point, (Point)g[v].point);
                svg_graph.draw(l,  "blue", scale_(0.07));
                last_point = (Point)g[v].point;
                if(predmap[v] == v)
                    break;
            }

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
        if(surfaces[print_z]->contains_b(vertex_p)) { // is this point inside infill?
            Graph& g_i = const_cast<Graph&>(g);
            Points pts;

            // generate grid candidates
            Point spacing(step_distance, step_distance);
            Point base(0, 0);
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
            vertex_iterator i, iend;
            coord_t diagonal_distance = sqrt(step_distance*step_distance*2);
            for (boost::tie(i, iend) = boost::vertices(g); i != iend; ++i) {
                if(g[*i].point.z == print_z) {
                    if(vertex_p.distance_to((Point)g[*i].point) < diagonal_distance) {
                        pts.push_back((Point)g[*i].point);
                    }
                }
            }

            for(auto &p : pts) {
                if(surfaces[print_z]->contains_b(p)) { // is this point inside infill?
                    Vertex v;
                    Slic3r::add_point_vertex(Point3(p, print_z), g_i, *point_index, &v);
                    coord_t distance = vertex_p.distance_to(p) * routing_explore_weight_factor;
                    // avoid self-loops
                    if(u != v) {
                        boost::add_edge(u, v, distance, g_i);
                    }
                }
            }
        }

        // traverse back (for overlap) and check for matching points in the neighbor-layers
        boost::property_map<routing_graph_t, boost::edge_weight_t>::type EdgeWeightMap = boost::get(boost::edge_weight_t(), g_i);
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
        vertex_t last_vertex = u;
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
            // do we have a matching line?
      /*      if(traverse_upper && upper_trace.length() >= this->layer_overlap) {
                std::cout << "found matching upper line" << std::endl;
                // debug output graph
                std::ostringstream ss;
                ss << "graph_visitor_";
                ss << print_z;
                ss << ".svg";
                std::string filename = ss.str();
                SVG svg_graph(filename.c_str());
//                SVG svg_graph("graph_visitor", BoundingBox(Point(0, 0), Point(scale_(50), scale_(50))));
                svg_graph.draw(*surfaces[print_z], "black");
                boost::graph_traits<routing_graph_t>::edge_iterator ei, ei_end;
                for (boost::tie(ei, ei_end) = edges(g); ei != ei_end; ++ei) {
                    if(g[boost::source(*ei, g)].point.z == print_z || g[boost::target(*ei, g)].point.z == print_z) {
                        Line l = Line((Point)g[boost::source(*ei, g)].point, (Point)g[boost::target(*ei, g)].point);
                        boost::property_map<routing_graph_t, boost::edge_weight_t>::type EdgeWeightMap = boost::get(boost::edge_weight_t(), g_i);
                        coord_t weight = boost::get(EdgeWeightMap, *ei);
                        double w = weight / l.length();
                        w -=0.99;
                        w = w*50;
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
                }

                // draw current route
                auto predmap = boost::get(&PointVertex::predecessor, g);
                Point last_point = (Point)g[u].point;
                for(routing_vertex_t v = u;; v = predmap[v]) {
                    Line l = Line(last_point, (Point)g[v].point);
                    svg_graph.draw(l,  "green", scale_(0.15));
                    svg_graph.draw((Point)g[v].point, "green", scale_(0.3));
                    last_point = (Point)g[v].point;
                    if(predmap[v] == v)
                        break;
                }

                // draw new connection
                svg_graph.draw(upper_trace,  "blue", scale_(0.15));
                for(Point &p : upper_trace.points) {
                    svg_graph.draw(p, "blue", scale_(0.3));
                }

                svg_graph.Close();

                char ch;
                std::cin >> ch;
            }
*/
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
