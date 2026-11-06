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

            /* True if the part of a resource which an access reaches is all of it. */
            static bool covers_whole_resource(const Analysis::APIDumpAnalysis::SubresourceRange&    in_range,
                                              const Analysis::APIDumpAnalysis::ResourceDescription& in_description);

            /* The stronger of two ways of using what was left: CERTAIN, then POSSIBLE, then NONE. */
            static CarryKind merge_carry(const CarryKind& in_a,
                                         const CarryKind& in_b);

            static void analyze_resource(const AccessLog&                                      in_log,
                                         const std::vector<uint32_t>&                          in_frame_index_vec,
                                         const ResourceAccesses&                               in_accesses,
                                         const Analysis::APIDumpAnalysis::ResourceDescription& in_description,
                                         ResourceDependencies*                                 out_dependencies_ptr);
        };
    }
}

#endif /* APIDUMPDEPENDENCIES_DEPENDENCY_ANALYZER_H */
