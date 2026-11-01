/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREOREPLAYENGINEDLL_FRAME_PACER_H
#define STEREOREPLAYENGINEDLL_FRAME_PACER_H

#include <atomic>
#include <stdint.h>

namespace Replay
{
    namespace StereoReplayEngineDLL
    {
        /* Presents the frames at the pace at which the dumped process did: waits for the time that the frame was presented
         * at, relative to the previous one. A gap much longer than the usual one, or longer than a fixed limit, is cut
         * short, because the long ones are mostly the dumper's own (writing a segment), not the app's. A replay which
         * falls behind does not catch up in a burst: it starts the timeline anew.
         */
        class FramePacer
        {
        public:
            FramePacer();

            /* The timeline starts anew with the next frame (after a pause, or a reset). */
            void reset();

            /* 1 is the pace of the dumped process, 2 is twice as fast, 0 is as fast as possible. Any thread. */
            void set_speed(const double& in_speed);

            /* Called for each frame, from the replay thread, with the time stamp of the call that ends it. */
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

#endif /* STEREOREPLAYENGINEDLL_FRAME_PACER_H */
