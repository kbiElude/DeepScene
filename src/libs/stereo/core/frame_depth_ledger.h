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
        /* Keeps what a converter has of how deep its frames are, when the depth buffer is not read at once: a read of it that is not waited for (to a buffer of the graphics card)
         * is a result that is in some frames later, and the result of a pass is of the frame that it was made in. The ledger has the reads that were started and have no result
         * yet, and the depth of each frame so far (the farthest and the nearest of the passes, the median of their medians, the x scale of the last), and gives the depth of the
         * frame that ended a given number of frames ago: with none the depth is the one of the frame that has just ended (a read that is waited for), with one it is the
         * one of the frame before. Which results are waited for is the adapter's: it takes the reads that are older than the frame it is in when the frame ends.
         * API-neutral. Not thread-safe.
         */
        class FrameDepthLedger
        {
        public:
            FrameDepthLedger();

            /* A read is started in the frame that is being made, for the pass whose projection has the scales. */
            void add_pending(const uint32_t& in_slot,
                             const float&    in_x_scale,
                             const float&    in_z_scale,
                             const float&    in_z_shift);

            /* The result of a read that was started (see pop_pending()) is in: it is added to the frame it was started in, if the ledger still has it. */
            void add_result(const PendingDepthRead& in_read,
                            const DepthRange&       in_range);

            /* The result of a read that was waited for, of the frame that is being made. */
            void add_result_now(const DepthRange& in_range,
                                const float&      in_x_scale);

            /* The frame that is being made ends, the depth of the one that ended this many frames ago is ready (there is none for the first frames, or for a frame in which
             * no pass was read), and the next frame begins. A depth that was ready and was not taken is dropped.
             */
            void end_frame(const uint32_t& in_lag);

            /* Forgets the reads that were started: their places are gone (the buffers were made again). What was added of the frames stays. */
            void forget_pending();

            uint64_t get_frame() const;
            size_t   get_n_pending() const;

            /* Takes the oldest read that was started, if it was started before the frame that is being made, or if it is not asked for only those; false if there is none. */
            bool pop_pending(const bool&       in_only_older_frames,
                             PendingDepthRead* out_read_ptr);

            /* The depth of the frame that is ready, once: the farthest and the nearest of its passes, the median of their medians, and the x scale of the last; false if there
             * is none.
             */
            bool take_ready(DepthRange* out_range_ptr,
                            float*      out_x_scale_ptr);

        private:
            /* What the passes of a frame say, as far as the results are in. */
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
            uint64_t                       m_oldest_kept_frame; /* the results of the frames before it are of no use: they were given, or dropped */
            std::deque<PendingDepthRead>   m_pending_read_deque;
            FrameDepth                     m_ready;
        };
    }
}

#endif /* STEREO_CORE_FRAME_DEPTH_LEDGER_H */
