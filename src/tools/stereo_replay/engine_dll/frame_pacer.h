/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_REPLAY_ENGINEDLL_FRAME_PACER_H
#define STEREO_REPLAY_ENGINEDLL_FRAME_PACER_H

#include <atomic>
#include <stdint.h>

namespace Stereo
{
    namespace Replay
    {
        namespace EngineDLL
        {
            /* Presents the frames at the pace of the dumped process. It waits until the time the frame was presented at, measured
             * relative to the previous frame. A gap is cut short if it is longer than a fixed limit or much longer than the usual gap,
             * because long gaps mostly come from the dumper (it writes a segment), not from the application. A replay that falls
             * behind does not catch up in a burst; it restarts the timeline.
             */
            class FramePacer
            {
            public:
                FramePacer();

                /* Restarts the timeline at the next frame. Call it after a pause or a reset. */
                void reset();

                /* 1 is the pace of the dumped process, 2 is twice as fast, 0 is as fast as possible. Any thread. */
                void set_speed(const double& in_speed);

                /* Waits for the presentation time of a frame. Called once per frame from the replay thread, with the time stamp of the
                 * call that ends the frame.
                 */
                void wait_for_frame(const uint64_t& in_timestamp_qpc,
                                    const uint64_t& in_qpc_frequency);

            private:
                static uint64_t get_wall_frequency();
                static uint64_t get_wall_ticks    ();

                double              m_average_gap_seconds;
                bool                m_is_anchored;
                uint64_t            m_last_timestamp_qpc;
                uint64_t            m_target_wall_ticks;
                std::atomic<double> m_speed;
            };
        }
    }
}

#endif /* STEREO_REPLAY_ENGINEDLL_FRAME_PACER_H */
