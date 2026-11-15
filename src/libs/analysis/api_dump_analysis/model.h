/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * The analysis of a workload. Its size grows with the variety of what the application did, not with the length of the capture: a frame is a
 * small record that points to its shape, and only a few frames of each shape keep their detail.
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
            /* The camera constants over time, per pass structure. */
            std::map<ConstantSeriesKey, ValueTimeSeries> constant_series_map;

            /* The depth of the draws of each pass structure as the camera of the pass saw them, for each kind of frame the pass occurs in
             * (the same pass in one scene differs from that in another). A pass with no draw in front of the camera has no entry.
             */
            std::map<DepthHistogramKey, DepthHistogram> depth_histogram_map;

            /* What the draws of each kind did (see DrawFacts), by the key of their structure and size. Empty if the analysis did not collect it. */
            std::map<DrawFactsKey, DrawFacts> draw_facts_map;

            /* Draw structures by key. */
            std::map<uint64_t, DrawStructure> draw_structure_map;

            std::vector<FrameRecord>         frame_record_vec;
            StructureInterner<FrameShape>    frame_shape_interner;
            uint64_t                         n_key_collisions = 0; /* the number of different draw structures that have the same key */
            StructureInterner<PassStructure> pass_structure_interner;
            RepresentativeStore              representative_store;
            std::vector<ResourceAccess>      resource_access_vec;  /* indexed by ID - 1 */
            std::vector<Resource>            resource_vec;         /* indexed by ID - 1 */

            /* The depth of the surfaces that the draws of each pass structure draw (the vertices, not the origins of the objects) as the camera of
             * the pass saw them, for each kind of frame the pass occurs in. A pass whose front end could not read the vertices has no entry.
             */
            std::map<DepthHistogramKey, DepthHistogram> surface_depth_histogram_map;

            /* The width of the picture for each pass of each kind of frame (see ProjectionScale), for the draws with a perspective projection. */
            std::map<DepthHistogramKey, ProjectionScale> projection_scale_map;

            /* The keys of the draws that cover the whole view (a flash, a fade; see KeyBuilder::compute_draw_cover_key(): a signature and a size),
             * per pass structure. A draw key counts as covering if, in at least one draw with that key, the vertices reach all four edges of the
             * picture through the projection of the pass (a quad that wipes in covers the whole picture only for a while). Moving such a draw
             * sideways would leave a strip of the picture bare, so it must not be moved.
             *
             * The map is per pass structure because a scene can contain a mesh that covers the view and has the same key as the quad of an
             * overlay, and what the recipe leaves alone is decided per pass.
             */
            std::map<uint32_t, std::set<uint64_t> > covering_draw_key_map; /* by the pass structure of the draws */
        };

        /* A model together with what it was made from: the contents of a .passes file. */
        struct AnalysisResult
        {
            Model             model;
            WorkloadSummary   summary;
            WorkloadReference workload_reference;
        };
    }
}

#endif /* APIDUMPANALYSIS_MODEL_H */
