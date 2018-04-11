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
        //routed_wire.append(unrouted_wire.first_point());// add first point

        // generate set of inflated expolygons. Inflate by perimeter width per region and channel width.
        std::vector<ExPolygonCollection> inflated_slices;

        // setup graph structure for routing
        routing_graph_t routing_graph;
        boost::property_map<routing_graph_t, boost::vertex_index_t>::type routing_graph_index = boost::get(boost::vertex_index, routing_graph); // Index Vertex->Point
        point_index_t graph_point_index; // Index Point->Vertex

        for(int i=0; i < max_perimeters; i++) {
            // initialize vector element
            inflated_slices.push_back(ExPolygonCollection());

            for(const auto &region : this->layer->regions) {
                const coord_t perimeters_thickness = this->offset_width(region, i+1);
                inflated_slices[i].append(offset_ex((Polygons)region->slices, -perimeters_thickness));
            }
        }
        if(debug_flag) {
            int c = 50;
            for(int i=0; i < max_perimeters; i++) {
                std::ostringstream ss;
                ss << "rgb(" << 100 << "," << c << "," << c << ")";
                std::string color = ss.str();
                svg.draw(inflated_slices[i], color);
                c += 50;
            }
            debug_flag = false;
        }


        std::vector<WireSegment> segments;
        double edge_weight_factor = 1.0 + max_perimeters * 0.1;
        for(auto &line : unrouted_wire.lines()) {
            WireSegment segment;
            segment.line = line;
            for(int current_perimeters = 0; current_perimeters < max_perimeters; current_perimeters++) {
                Line iteration_line = line;
                Line collision_line = line; // reset line for next iteration
                while(true) {
                    bool outside_expolygon = false;
                    Point intersection;
                    bool ccw;
                    Polygon* p;
                    ExPolygon* ep;
                    // find intersections with all expolygons
                    while(inflated_slices[current_perimeters].first_intersection(collision_line, &intersection, &ccw, &p, &ep)) {
                        p->insert(intersection);
                        if(ccw) { // leaving expolygon
                            outside_expolygon = true;
                            //svg.draw(Line(line.a, intersection), "red", scale_(this->extrusion_width));
                            Line l(iteration_line.a, intersection);
                            segment.connecting_lines.push_back(std::make_pair(l, edge_weight_factor));
                            //iteration_line.a = intersection;
                            //p->insert(intersection);
                        }else{ // entering expolygon
                            Line l(iteration_line.a, intersection);
                            if(!outside_expolygon) {
                                //iteration_line.a = intersection;
                                //p->insert(intersection);
                                segment.connecting_lines.push_back(std::make_pair(l, edge_weight_factor*2.0));
                            }// else do nothing... we are only looking for connections inside polygons
                            outside_expolygon = false;
                        }
                        iteration_line.a = intersection;
                        collision_line.a = intersection;
                        collision_line.extend_start(-scale_(EPSILON)); // crop line a bit to avoid multiple intersections with the same polygon
                    }
                    // no further intersections found, rest must be inside or outside of slices. Append linear line
                    segment.connecting_lines.push_back(std::make_pair(iteration_line, edge_weight_factor));
                    break;
                }
            }
            segments.push_back(segment);
            for(int i=0; i<segment.connecting_lines.size(); i++) {
                //Line l(segment.connecting_lines[i].first);
                //l.translate(scale_(i*0.3), 0);
                //svg.draw(l, "red", scale_(this->extrusion_width/4));
            }
            for(int i=0; i<segment.connecting_lines.size(); i++) {
                svg.draw(segment.connecting_lines[i].first.a, "black", scale_(this->extrusion_width/4));
                svg.draw(segment.connecting_lines[i].first.b, "black", scale_(this->extrusion_width/4));
            }
        }

        // add contour polygons to graph
        //for(ExPolygons &eps : inflated_slices) {
        coord_t perimeter_sum = 0;
        for(int i = 0; i < max_perimeters; i++) {
            //double edge_weight_factor = 2 * perimeter_sum * PI; //2 * r * pi ->
            // 2*r <-> 2*r*pi
            //std::cout << "edge_weight_factor: " << edge_weight_factor << std::endl;
            double edge_weight_factor = 1.0 + (max_perimeters-i-1) * 0.1;
            for(ExPolygon &ep : (ExPolygons)inflated_slices[i]) {
                // iterate over contour and holes to avoid deep copy
                this->polygon_to_graph(ep.contour, &routing_graph, &graph_point_index, edge_weight_factor);
                for(Polygon &p : ep.holes) {
                    this->polygon_to_graph(p, &routing_graph, &graph_point_index, edge_weight_factor);
                }
            }
            //perimeter_sum += perimeters_thicknesses[i];
        }

/*        std::ofstream file;
        file.open("graph.dot");
        boost::property_map<routing_graph_t, boost::edge_weight_t>::type EdgeWeightMap = get(boost::edge_weight_t(), routing_graph);
        if (file.is_open()) {
            boost::write_graphviz(file, routing_graph,
                boost::make_label_writer(routing_graph_index),
                boost::make_label_writer(EdgeWeightMap));
        }
*/

        // debug output graph
        std::ostringstream ss1;
        ss1 << "graph_debug_";
        ss1 << index;
        ss1 << ".svg";
        filename = ss1.str();
        SVG svg_graph(filename.c_str());
        svg_graph.draw(*(this->slices), "black");
        boost::graph_traits<routing_graph_t>::edge_iterator ei, ei_end;
        for (boost::tie(ei, ei_end) = edges(routing_graph); ei != ei_end; ++ei) {

            Point a = routing_graph_index[boost::source(*ei, routing_graph)];
            Point b = routing_graph_index[boost::target(*ei, routing_graph)];
            Line l = Line(a, b);
            svg_graph.draw(l, "red", scale_(this->extrusion_width/4));
        }

        // A* search on routing graph
        for(auto segment : segments) {

            // add connecting lines to graph
            for(int i=0; i<segment.connecting_lines.size(); i++) {
                double edge_weight_factor = segment.connecting_lines[i].second;

                // add endpoints to graph. Won't add multiple copies of the same point due to hash structure.
                routing_edge_t edge;
                Line l = segment.connecting_lines[i].first;
                this->add_vertex(l.a, &routing_graph, &graph_point_index);
                this->add_vertex(l.b, &routing_graph, &graph_point_index);
                this->add_edge(l, &routing_graph, &graph_point_index, &edge, edge_weight_factor);
                Line dbg = l;
                dbg.translate(scale_(i*0.5), 0);
                svg.draw(dbg, "red", scale_(this->extrusion_width/8));
                segment.edges.insert(edge);
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
                        ss << "rgb(" << 100 << "," << edge_weight_factor*100 << "," << edge_weight_factor*100 << ")";
                        std::string color = ss.str();
                        // add points of 2nd line
                        this->add_vertex(segment.connecting_lines[j].first.a, &routing_graph, &graph_point_index);
                        this->add_vertex(segment.connecting_lines[j].first.b, &routing_graph, &graph_point_index);
                        // add the remaining combination of edges. 2nd line will be added later in the loop.
                        this->add_edge(Line(segment.connecting_lines[i].first.a, segment.connecting_lines[j].first.b), &routing_graph, &graph_point_index, &edge, edge_weight_factor);
                        Line d = Line(segment.connecting_lines[i].first.a, segment.connecting_lines[j].first.b);
                        d.translate(scale_(i*0.5+0.1+debug_offset), 0);
                        svg.draw(d, color, scale_(this->extrusion_width/6));
                        segment.edges.insert(edge);
                        this->add_edge(Line(segment.connecting_lines[j].first.a, segment.connecting_lines[i].first.b), &routing_graph, &graph_point_index, &edge, edge_weight_factor);
                        segment.edges.insert(edge);
                        d = Line(segment.connecting_lines[j].first.a, segment.connecting_lines[i].first.b);
                        d.translate(scale_(i*0.5+0.2+debug_offset), 0);
                        svg.draw(d, color, scale_(this->extrusion_width/6));
                        debug_offset += 0.2;
                    }
                }
            }

            // generate an index-map to map vertex descriptors to array indices
            boost::property_map<routing_graph_t, boost::vertex_index1_t>::type indexmap = boost::get(boost::vertex_index1, routing_graph);
            boost::graph_traits<routing_graph_t>::vertex_iterator i, iend;
            int idx = 0;
            for (boost::tie(i, iend) = boost::vertices(routing_graph); i != iend; ++i, ++idx) {
                indexmap[*i] = idx;
            }

            std::vector<routing_graph_t::vertex_descriptor> predmap(boost::num_vertices(routing_graph));
            //std::vector<coord_t> distmap(boost::num_vertices(routing_graph));
            routing_vertex_t start = graph_point_index[segment.line.b];
            routing_vertex_t goal = graph_point_index[segment.line.a];

            try {
                // call astar named parameter interface
                astar_search
                (routing_graph, start,
                    distance_heuristic<routing_graph_t, coord_t, boost::property_map<routing_graph_t, boost::vertex_index_t>::type>
                    (routing_graph_index, goal),
                    vertex_index_map(indexmap).
                    predecessor_map(boost::make_iterator_property_map(predmap.begin(), boost::get(boost::vertex_index1, routing_graph))).
                    //distance_map(boost::make_iterator_property_map(distmap.begin(), boost::get(boost::vertex_index1, routing_graph))).
                    visitor(astar_goal_visitor<routing_vertex_t>(goal))
                    );


            } catch(found_goal fg) { // found a path to the goal
                for(routing_vertex_t v = goal;; v = predmap[indexmap[v]]) {
                    if(predmap[indexmap[v]] == v)
                        break;
                    routed_wire.append(routing_graph_index[v]);
                }
            }



            // remove edges generated by this segment to avoid double usage when routing the next segment
            for(auto edge : segment.edges) {
                boost::remove_edge(edge, routing_graph);
            }
        }

        // add final point
        routed_wire.append(unrouted_wire.last_point());

        svg_graph.draw(routed_wire, "yellow", scale_(this->extrusion_width/8));
        svg_graph.arrows = false;
        svg_graph.Close();

        //svg.draw(routed_wire, "yellow", scale_(this->extrusion_width));
        //routed_wire.remove_loops();
        this->channel_from_wire(routed_wire);
        this->routed_wires.push_back(routed_wire);
        svg.draw(routed_wire, "white", scale_(this->extrusion_width)/4);

        double index = 0;
        for(auto point : routed_wire.points) {
            point.translate(scale_(index), 0.0);
            svg.draw(point, "green", scale_(this->extrusion_width/6));
            //svg.draw(wire.b, "green", scale_(this->extrusion_width/6));
            index += 0.2;
        }

        svg.arrows = false;
        svg.Close();
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
            Polygon* p;
            ExPolygon* ep;
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

void ElectronicWireGenerator::polygon_to_graph(const Polygon& p, routing_graph_t* g, point_index_t* index, const double& weight_factor) const
{
    if(p.is_valid()) {
        routing_vertex_t last_vertex, this_vertex, first_vertex;
        last_vertex = boost::add_vertex(p.points.front(), *g); // add first point
        (*index)[p.points.front()] = last_vertex;
        first_vertex = last_vertex;
        for (Points::const_iterator it = p.points.begin(); it != p.points.end()-1; ++it) {
            // add next vertex
            this_vertex = boost::add_vertex(*(it+1), *g);
            (*index)[*(it+1)] = this_vertex;

            // add edge
            coord_t distance = it->distance_to(*(it+1)) * weight_factor;
            // Add factor to prioritize routing with high distance to external perimeter
            boost::add_edge(last_vertex, this_vertex, distance, *g);
            last_vertex = this_vertex;
        }
        // add final edge to close the loop
        coord_t distance = p.points.back().distance_to(p.points.front());
        // Add factor to prioritize routing with high distance to external perimeter
        boost::add_edge(last_vertex, first_vertex, distance, *g);
    }
}

bool ElectronicWireGenerator::add_vertex(const Point& p, routing_graph_t* g, point_index_t* index) const
{
    bool result = false;
    if((*index).find(p) == (*index).end()) { // is this point already in the graph?
        routing_vertex_t vertex = boost::add_vertex(p, *g);
        (*index)[p] = vertex; // add vertex to reverse lookup table
        result = true;
    }
    return result;
}


bool ElectronicWireGenerator::add_edge(const Line& l, routing_graph_t* g, point_index_t* index, routing_edge_t* edge, const double& edge_weight_factor) const
{
    bool result = true;
    try {
        routing_vertex_t vertex_a = (*index).at(l.a); // unordered_map::at throws an out-of-range
        routing_vertex_t vertex_b = (*index).at(l.b); // unordered_map::at throws an out-of-range
        coord_t distance = l.length() * edge_weight_factor;
        *edge = boost::add_edge(vertex_a, vertex_b, distance, *g).first;
    } catch (const std::out_of_range& oor) {
        std::cerr << "Unable to find vertex for point while adding an edge: " << oor.what() << '\n';
        result = false;
    }
    return result;
}

}
