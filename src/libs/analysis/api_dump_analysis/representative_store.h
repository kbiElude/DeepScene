/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Keeps the detail of a few frames of each shape, instead of every frame: the first one, and the ones with the least and
 * the most weight (e.g. the fewest and the most draws). What the analysis keeps is therefore bounded by the number of
 * shapes, not by the length of the capture. The detail of a frame costs time to build, so wants() tells whether the frame
 * would be kept before it is built.
 */
#ifndef APIDUMPANALYSIS_REPRESENTATIVE_STORE_H
#define APIDUMPANALYSIS_REPRESENTATIVE_STORE_H

#include "analysis/api_dump_analysis/types.h"
#include <map>
#include <stdint.h>
#include <vector>

namespace Analysis
{
    namespace APIDumpAnalysis
    {
        class RepresentativeStore
        {
        public:
            struct Entry
            {
                FrameDetail detail;
                uint64_t    frame_index = 0;
                uint64_t    weight      = 0;
            };

            typedef std::vector<const Entry*> EntryPtrVector;

            RepresentativeStore();

            /* The caller must have asked wants() for the same frame. */
            void add(const uint32_t&    in_shape_id,
                     const uint64_t&    in_frame_index,
                     const uint64_t&    in_weight,
                     const FrameDetail& in_detail);

            /* The first frame of the shape, then the lightest and the heaviest if those are other frames. */
            EntryPtrVector get_entry_ptr_vec(const uint32_t& in_shape_id) const;

            /* True if the frame is the first of its shape, or lighter or heavier than those kept so far. */
            bool wants(const uint32_t& in_shape_id,
                       const uint64_t& in_weight) const;

        private:
            struct Slot
            {
                Entry    first;
                Entry    heaviest;
                bool     has_heaviest = false;
                bool     has_lightest = false;
                Entry    lightest;
                uint64_t max_weight   = 0;
                uint64_t min_weight   = 0;
            };

            std::map<uint32_t, Slot> m_slot_map;
        };
    }
}

#endif /* APIDUMPANALYSIS_REPRESENTATIVE_STORE_H */
