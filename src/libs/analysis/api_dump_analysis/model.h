/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * The analysis of a workload. Its size follows the variety of what the application did, not the length of the capture: a frame is
 * a small record pointing to its shape, and only a few frames of each shape keep their detail.
 */
#ifndef APIDUMPANALYSIS_MODEL_H
#define APIDUMPANALYSIS_MODEL_H

#include "analysis/api_dump_analysis/depth_histogram.h"
#include "analysis/api_dump_analysis/representative_store.h"
#include "analysis/api_dump_analysis/structure_interner.h"
#include "analysis/api_dump_analysis/types.h"
#include "analysis/api_dump_analysis/value_time_series.h"
#include <map>
#include <set>
#include <utility>
#include <vector>

namespace Analysis
{
    namespace APIDumpAnalysis
    {
        struct Model
        {
            /* Camera constants over time, per pass structure. */
            std::map<ConstantSeriesKey, ValueTimeSeries> constant_series_map;

            /* How deep the draws of each pass structure are, as the camera of the pass saw them, in each kind of frame it is in (a pass of
             * one scene is not one of another). A pass with no draw in front of the camera has none.
             */
            std::map<DepthHistogramKey, DepthHistogram> depth_histogram_map;

            /* Draw structures by key. */
            std::map<uint64_t, DrawStructure> draw_structure_map;

            std::vector<FrameRecord>         frame_record_vec;
            StructureInterner<FrameShape>    frame_shape_interner;
            uint64_t                         n_key_collisions = 0; /* different draw structures with the same key      */
            StructureInterner<PassStructure> pass_structure_interner;
            RepresentativeStore              representative_store;
            std::vector<ResourceAccess>      resource_access_vec;  /* by ID, less 1 */
            std::vector<Resource>            resource_vec;         /* by ID, less 1 */

            /* How deep the surfaces that the draws of each pass structure draw are (the vertices, not the origins of the objects), as the
             * camera of the pass saw them, in each kind of frame it is in. A pass whose front end could not tell the vertices has none.
             */
            std::map<DepthHistogramKey, DepthHistogram> surface_depth_histogram_map;

            /* By the pass structure that the draws were made in: a scene can have a mesh that covers the view with the key of an overlay's quad,
             * and what the recipe leaves alone is told by the pass.
             *
             * The keys of the draws that cover the whole of the view (a flash, a fade; see KeyBuilder::compute_draw_cover_key(): a signature and a
             * size): their vertices, through the projection of the pass, reach all four edges of the picture, in at least one draw with the key
             * (a quad that wipes in is the whole picture only for a time). What is moved sideways leaves a strip of the picture bare, so a draw like it is not to be moved.
             */
            std::map<uint32_t, std::set<uint64_t> > covering_draw_key_map; /* by the pass structure the draws were in */
        };

        /* A model and what it was made from: the contents of a .passes file. */
        struct AnalysisResult
        {
            Model             model;
            WorkloadSummary   summary;
            WorkloadReference workload_reference;
        };
    }
}

#endif /* APIDUMPANALYSIS_MODEL_H */
