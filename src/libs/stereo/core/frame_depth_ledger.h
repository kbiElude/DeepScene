/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_CORE_FRAME_DEPTH_LEDGER_H
#define STEREO_CORE_FRAME_DEPTH_LEDGER_H

#include "stereo/core/depth_buffer_statistics.h"
#include "stereo/core/types.h"
#include <deque>
#include <map>
#include <stdint.h>
#include <vector>

namespace Stereo
{
    namespace Core
    {
        /* Collects the depth of each frame for a converter that does not wait for its depth buffer reads. A read that is not waited for (into
         * a graphics card buffer) delivers its result some frames later, and the result belongs to the frame in which the pass was made. The
         * ledger holds the reads that were started and have no result yet. It also holds the depth of each frame so far: the farthest and the
         * nearest depth over the passes, the median of the passes' medians, and the x scale of the last pass. It hands out the depth of the
         * frame that ended a given number of frames ago (the lag). With lag 0 that is the frame that has just ended (its reads were waited
         * for), with lag 1 it is the frame before. The adapter decides which results to wait for: when a frame ends, it takes the reads that
         * started in earlier frames.
         * API-neutral. Not thread-safe.
         */
        class FrameDepthLedger
        {
        public:
            FrameDepthLedger();

            /* Records a read that was started in the frame being made, for the pass whose projection has these scales. in_slot is the place
             * where the adapter holds the result.
             */
            void add_pending(const uint32_t& in_slot,
                             const float&    in_x_scale,
                             const float&    in_z_scale,
                             const float&    in_z_shift);

            /* Adds the result of a read that was started (see pop_pending()) to the frame in which it was started. The result is dropped if the
             * ledger no longer has that frame.
             */
            void add_result(const PendingDepthRead& in_read,
                            const DepthRange&       in_range);

            /* Adds the result of a read that was waited for to the frame being made. */
            void add_result_now(const DepthRange& in_range,
                                const float&      in_x_scale);

            /* Ends the frame being made and begins the next one. The depth of the frame that ended in_lag frames ago becomes ready for
             * take_ready(). No depth is ready for the first frames, or for a frame in which no pass was read. A ready depth that was not taken
             * is dropped.
             */
            void end_frame(const uint32_t& in_lag);

            /* Drops the reads that were started, because their slots no longer exist (the buffers were created again). The results that were
             * already added to frames stay.
             */
            void forget_pending();

            uint64_t get_frame() const;
            size_t   get_n_pending() const;

            /* Removes the oldest pending read and returns it in out_read_ptr. If in_only_older_frames is true, only a read that started before
             * the frame being made qualifies. Returns false if there is no such read.
             */
            bool pop_pending(const bool&       in_only_older_frames,
                             PendingDepthRead* out_read_ptr);

            /* Takes the depth of the ready frame. It can be taken once. The depth is the farthest and the nearest depth over the passes of the
             * frame, the median of the passes' medians, and the x scale of the last pass. Returns false if no depth is ready.
             */
            bool take_ready(DepthRange* out_range_ptr,
                            float*      out_x_scale_ptr);

        private:
            /* What the passes of a frame say, as far as their results have arrived. */
            struct FrameDepth
            {
                bool               has_depth = false;
                std::vector<float> median_vec;
                DepthRange         range;
                float              x_scale   = 0.0f;
            };

            void add_to_frame(const uint64_t&   in_frame,
                              const DepthRange& in_range,
                              const float&      in_x_scale);

            uint64_t                       m_frame;
            std::map<uint64_t, FrameDepth> m_frame_map;
            uint64_t                       m_oldest_kept_frame; /* results of earlier frames are not needed: they were taken or dropped */
            std::deque<PendingDepthRead>   m_pending_read_deque;
            FrameDepth                     m_ready;
        };
    }
}

#endif /* STEREO_CORE_FRAME_DEPTH_LEDGER_H */
