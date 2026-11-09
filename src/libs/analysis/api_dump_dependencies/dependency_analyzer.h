/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Works out from the accesses to the resources of a workload what each frame does with what the frames before it left in them: whether it uses it
 * (reads it, blends into it, tests against it, or writes only a part of it and reads on), and which frame wrote what it uses.
 *
 * A frame uses what was left in a resource when something reads the resource (or a write blends or tests with it) before a write has replaced all
 * of its content. A write which is not known to reach all of the content (a draw, a copy of a rectangle, a clear of the viewport when the viewport is
 * less than the target) replaces nothing: what the frame reads after it may be old content, and the frame is said to use it only possibly. That can
 * only over-report what is carried over from frame to frame, never under-report it.
 *
 * The resources are independent of each other, so they are worked on by several threads.
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
            /* The resources are as the model builder made them, by the number less 1 (see Analysis::APIDumpAnalysis::Model::resource_vec); the
             * accesses may name no others.
             */
            static DependencyResult analyze(const AccessLog&                                        in_log,
                                            const std::vector<Analysis::APIDumpAnalysis::Resource>& in_resource_vec,
                                            const uint32_t&                                         in_n_threads);

        private:
            /* The accesses to one resource: the indices (in the log) of those from the first to the end, in the order they were made in. */
            struct ResourceAccesses
            {
                const uint32_t* event_index_ptr = nullptr;
                size_t          n_events        = 0;
            };

            /* What the calls of a frame carry from the resources to each other, for the resources that have a written extent (see compute_inputs()). */
            struct InputWorkspace
            {
                std::vector<std::vector<uint32_t> > dependency_vec_vec;  /* by resource number: those whose content before the frame flows into it, in order */
                std::vector<uint8_t>                has_dependencies_vec;/* by resource number: written in the frame (dependency_vec_vec is what it is)   */
                std::vector<uint8_t>                has_feedback_vec;    /* by resource number: its own content before the frame flows into a write to it  */
                std::vector<uint8_t>                is_written_vec;
                std::vector<uint32_t>               touched_vec;         /* the resources whose entries are not as they begin                              */
            };

            /* What a frame gave a resource that it wrote: the inputs of the version (see ResourceVersion::input_resource_vec), and whether the content that the
             * resource had before the frame flows into what the frame wrote to it.
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

            /* Follows what the calls of a frame make of what they read, in the order they are made in: a write makes the resource what the call read (a write which
             * replaces all of the resource, as that and nothing more; one that does not, as what it was and that), so that at the end of the frame the resource has
             * the content of the resources that flowed into it. Only the resources that have a written extent are told of.
             */
            static void compute_frame_inputs(const std::vector<Analysis::APIDumpAnalysis::AccessEvent>& in_event_vec,
                                             const size_t&                                               in_first_event,
                                             const size_t&                                               in_end_event,
                                             const std::vector<Analysis::APIDumpAnalysis::Resource>&     in_resource_vec,
                                             const std::vector<uint8_t>&                                 in_is_tracked_vec,
                                             InputWorkspace*                                             inout_workspace_ptr,
                                             std::vector<FrameInputs>*                                   out_frame_inputs_vec_ptr);

            /* Fills in the inputs of the versions of the resources that have a written extent. */
            static void compute_inputs(const AccessLog&                                        in_log,
                                       const std::vector<Analysis::APIDumpAnalysis::Resource>& in_resource_vec,
                                       const uint32_t&                                         in_n_threads,
                                       DependencyResult*                                       inout_result_ptr);

            /* The rectangle that holds all that the writes which do not replace all of the resource reached (see ResourceDependencies::written_extent);
             * not known if one of them did not say where it landed, or landed in a level other than the first.
             */
            static Analysis::APIDumpAnalysis::AccessRectangle compute_written_extent(const AccessLog&                                      in_log,
                                                                                     const ResourceAccesses&                               in_accesses,
                                                                                     const Analysis::APIDumpAnalysis::ResourceDescription& in_description);

            /* True if the part of a resource which an access reaches is all of it. */
            static bool covers_whole_resource(const Analysis::APIDumpAnalysis::SubresourceRange&    in_range,
                                              const Analysis::APIDumpAnalysis::ResourceDescription& in_description);

            /* True if the range is the first mip level of the first layer, which is where a rectangle of a write is told. */
            static bool is_first_level_range(const Analysis::APIDumpAnalysis::SubresourceRange& in_range);

            /* The stronger of two ways of using what was left: CERTAIN, then POSSIBLE, then NONE. */
            static CarryKind merge_carry(const CarryKind& in_a,
                                         const CarryKind& in_b);
        };
    }
}

#endif /* APIDUMPDEPENDENCIES_DEPENDENCY_ANALYZER_H */
