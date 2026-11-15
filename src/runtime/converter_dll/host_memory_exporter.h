/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_CONVERTERDLL_HOST_MEMORY_EXPORTER_H
#define STEREO_CONVERTERDLL_HOST_MEMORY_EXPORTER_H

#include "stereo/transport/frame_channel.h"
#include "stereo/transport/i_frame_exporter.h"
#include <stdint.h>

namespace Stereo
{
    namespace ConverterDLL
    {
        /* The base of the exporters that pass the images through the host memory of the frame channel. It offers only that transport kind, and it
         * waits for the viewer the same way for all of them.
         */
        class HostMemoryExporter : public Stereo::Transport::IFrameExporter
        {
        public:
            Stereo::Transport::TransportKindMask get_offered_kinds() const override;

        protected:
            /* The channel must outlive the exporter. The constructor registers the offered transport kind on the channel. */
            explicit HostMemoryExporter(Stereo::Transport::FrameChannel* inout_channel_ptr);

            Stereo::Transport::FrameChannel* get_channel_ptr() const;

            /* Blocks the calling thread until the next frame is due, so that at most in_n_frames_per_second frames per second pass through it. The first
             * frame is not held. It is for an application that has no screen to keep time by: the application then plays its music at the rate at which it is
             * shown. The thread sleeps in whole milliseconds, so the system timer must tick that often (timeBeginPeriod).
             */
            void hold_to_frame_rate(const int64_t& in_n_frames_per_second);

            /* Returns false until the other side reports that it accepts the host memory transport kind. */
            bool is_host_memory_negotiated() const;

            /* Makes the next frame that hold_to_frame_rate() sees count as the first one of a screen that keeps time, so it is not held. */
            void restart_frame_rate_clock();

            /* Waits until the screen wants a frame. Returns false if the wait timed out. */
            bool wait_for_screen();

        private:
            Stereo::Transport::FrameChannel* m_channel_ptr;
            int64_t                          m_last_frame_qpc; /* time of the last frame that was held to a rate or that started the clock; 0: none yet
 */
        };
    }
}

#endif /* STEREO_CONVERTERDLL_HOST_MEMORY_EXPORTER_H */
