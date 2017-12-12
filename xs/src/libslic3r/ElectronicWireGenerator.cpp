#include "ElectronicWireGenerator.hpp"
#include "ClipperUtils.hpp"

namespace Slic3r {

void
ElectronicWireGenerator::process()
{
    // contour following should happen at this point...

    // cut wires and apply overlaps
    this->apply_overlap(this->unrouted_wires, this->wires);


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

}
