/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Determines, for every frame and every resource, whether the frame uses the content that earlier frames left in the resource, and which frame
 * wrote that content.
 *
 * A frame uses the earlier content when an access reads the resource (or a write blends with it or tests against it) before a write has replaced
 * all of its content. A write that is not known to reach all of the content (a draw, a copy of a rectangle, a clear of a viewport smaller than the
 * target) replaces nothing. A read after such a write may see old content, so the use is reported as only possible. This can over-report what is
 * carried over from frame to frame, but never under-report it.
 *
 * The resources are independent of each other, so the analysis processes them on several threads.
 */
#ifndef APIDUMPDEPENDENCIES_DEPENDENCY_ANALYZER_H
#define APIDUMPDEPENDENCIES_DEPENDENCY_ANALYZER_H

#include "analysis/api_dump_dependencies/access_log.h"
#include "analysis/api_dump_dependencies/types.h"
#include <stdint.h>
#include <vector>

namespace Analysis
{
    namespace APIDumpDependencies
    {
        class DependencyAnalyzer
        {
        public:
            /* in_resource_vec is the resource list of the model builder. The number of a resource is its index plus 1 (see
             * Analysis::APIDumpAnalysis::Model::resource_vec). Accesses to a resource outside this list are ignored.
             */
            static DependencyResult analyze(const AccessLog&                                        in_log,
                                            const std::vector<Analysis::APIDumpAnalysis::Resource>& in_resource_vec,
                                            const uint32_t&                                         in_n_threads);

        private:
            /* The accesses to one resource: event_index_ptr points to n_events indices into the event vector of the log, in the order the
             * accesses were made.
             */
            struct ResourceAccesses
            {
                const uint32_t* event_index_ptr = nullptr;
                size_t          n_events        = 0;
            };

            /* Scratch state of compute_frame_inputs(): which resources flow into which within one frame. Only resources that have a written extent
             * are reported (see compute_inputs()). The vectors are indexed by resource number.
             */
            struct InputWorkspace
            {
                std::vector<std::vector<uint32_t> > dependency_vec_vec;  /* by resource number: the resources whose earlier content flows into it, sorted */
                std::vector<uint8_t>                has_dependencies_vec;/* by resource number: written in this frame, so dependency_vec_vec is valid     */
                std::vector<uint8_t>                has_feedback_vec;    /* by resource number: its own earlier content flows into a write to it          */
                std::vector<uint8_t>                is_written_vec;
                std::vector<uint32_t>               touched_vec;         /* the resources that must be reset at the end of the frame                      */
            };

            /* The result for one resource that a frame wrote: the inputs of the version the frame wrote (see ResourceVersion::input_resource_vec),
             * and whether the content that the resource had before the frame flows into the content the frame wrote.
             */
            struct FrameInputs
            {
                bool                  has_feedback = false;
                std::vector<uint32_t> input_resource_vec;
                uint32_t              resource_number = 0;
            };

            static void analyze_resource(const AccessLog&                                      in_log,
                                         const std::vector<uint32_t>&                          in_frame_index_vec,
                                         const ResourceAccesses&                               in_accesses,
                                         const Analysis::APIDumpAnalysis::ResourceDescription& in_description,
                                         ResourceDependencies*                                 out_dependencies_ptr);

            /* Follows the calls of one frame in order and tracks which resources flow into which. A write gives its resource the content that the
             * call read. A write that replaces the whole resource leaves only that content. A write that does not keeps the earlier content of the
             * resource as well. At the end of the frame, each written resource therefore depends on the resources whose earlier content flowed into
             * it. Only resources that have a written extent are tracked.
             */
            static void compute_frame_inputs(const std::vector<Analysis::APIDumpAnalysis::AccessEvent>& in_event_vec,
                                             const size_t&                                               in_first_event,
                                             const size_t&                                               in_end_event,
                                             const std::vector<Analysis::APIDumpAnalysis::Resource>&     in_resource_vec,
                                             const std::vector<uint8_t>&                                 in_is_tracked_vec,
                                             InputWorkspace*                                             inout_workspace_ptr,
                                             std::vector<FrameInputs>*                                   out_frame_inputs_vec_ptr);

            /* Fills in has_inputs and input_resource_vec of the versions of the resources that have a written extent. */
            static void compute_inputs(const AccessLog&                                        in_log,
                                       const std::vector<Analysis::APIDumpAnalysis::Resource>& in_resource_vec,
                                       const uint32_t&                                         in_n_threads,
                                       DependencyResult*                                       inout_result_ptr);

            /* The smallest rectangle that contains every write that does not replace the whole resource (see ResourceDependencies::written_extent).
             * It is unknown if any such write has no known rectangle or targets a level other than the first.
             */
            static Analysis::APIDumpAnalysis::AccessRectangle compute_written_extent(const AccessLog&                                      in_log,
                                                                                     const ResourceAccesses&                               in_accesses,
                                                                                     const Analysis::APIDumpAnalysis::ResourceDescription& in_description);

            /* True if the range covers every layer and mip level of the resource. */
            static bool covers_whole_resource(const Analysis::APIDumpAnalysis::SubresourceRange&    in_range,
                                              const Analysis::APIDumpAnalysis::ResourceDescription& in_description);

            /* True if the range is exactly the first mip level of the first layer. Write rectangles are recorded only for that range. */
            static bool is_first_level_range(const Analysis::APIDumpAnalysis::SubresourceRange& in_range);

            /* Returns the stronger of two carry kinds. The order is CERTAIN, then POSSIBLE, then NONE. */
            static CarryKind merge_carry(const CarryKind& in_a,
                                         const CarryKind& in_b);
        };
    }
}

#endif /* APIDUMPDEPENDENCIES_DEPENDENCY_ANALYZER_H */
