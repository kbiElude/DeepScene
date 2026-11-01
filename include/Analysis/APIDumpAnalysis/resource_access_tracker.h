/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Tracks what each frame reads and writes per resource, and totals it over the frames (see ResourceAccess). It tells whether a
 * frame can be played twice in a row: it cannot if it reads a resource before writing it and also writes it, since the second
 * run would read the first run's result instead of the previous frame's.
 *
 * A resource is treated as a whole: a mip level or a rectangle counts as the entire resource. This can only over-report
 * loop-carried resources, never under-report them.
 */
#ifndef APIDUMPANALYSIS_RESOURCE_ACCESS_TRACKER_H
#define APIDUMPANALYSIS_RESOURCE_ACCESS_TRACKER_H

#include "Analysis/APIDumpAnalysis/types.h"
#include <map>
#include <stdint.h>
#include <vector>

namespace Analysis
{
    namespace APIDumpAnalysis
    {
        class ResourceAccessTracker
        {
        public:
            ResourceAccessTracker();

            /* Adds the frame's accesses to the totals and starts the next frame. The vector has an entry for every resource
             * (ID - 1). The resources which are loop carried in this frame are listed, in the order of their IDs.
             */
            void end_frame(const uint64_t&              in_frame_index,
                           std::vector<ResourceAccess>* inout_access_vec_ptr,
                           std::vector<ResourceId>*     out_loop_carried_id_vec_ptr);

            /* The resource NONE is ignored. */
            void on_read (const ResourceId& in_id);
            void on_write(const ResourceId& in_id);


            /* The class of a resource, from its totals. */
            static ResourceAccessClass classify(const ResourceAccess& in_access);

            /* The number of resources of each class; every class has an entry. */
            static std::map<ResourceAccessClass, uint64_t> count_by_class(const std::vector<ResourceAccess>& in_access_vec);

        private:
            /* What the current frame has done to a resource so far. */
            struct FrameAccess
            {
                bool has_read              = false;
                bool has_read_before_write = false;
                bool has_write             = false;
            };

            FrameAccess* get_frame_access_ptr(const ResourceId& in_id);

            std::vector<FrameAccess> m_frame_access_vec; /* by ID - 1    */
            std::vector<uint32_t>    m_touched_vec;      /* indices of the entries above that this frame changed */
        };
    }
}

#endif /* APIDUMPANALYSIS_RESOURCE_ACCESS_TRACKER_H */
