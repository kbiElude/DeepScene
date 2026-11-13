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
        /* What the exporters which pass the images on by the memory of the channel have in common: that is all they offer, and they wait for the
         * program which shows the images the same way.
         */
        class HostMemoryExporter : public Stereo::Transport::IFrameExporter
        {
        public:
            Stereo::Transport::TransportKindMask get_offered_kinds() const override;

        protected:
            /* The channel must outlive the exporter. The exporter says on it what it offers. */
            explicit HostMemoryExporter(Stereo::Transport::FrameChannel* inout_channel_ptr);

            Stereo::Transport::FrameChannel* get_channel_ptr() const;

            /* Holds the thread until a frame is due, so that the frames that call it are at most this many a second (the first is not held). For an application that has no
             * screen to keep time by: what it does to the time of its music is then what it does when it is shown. The thread that calls it sleeps by the millisecond, which
             * needs the timer of the system to tick that often (timeBeginPeriod).
             */
            void hold_to_frame_rate(const int64_t& in_n_frames_per_second);

            /* False until the other side has said that it takes the host memory kind. */
            bool is_host_memory_negotiated() const;

            /* The next frame that hold_to_frame_rate() holds is counted from now, as a frame that a screen kept time for. */
            void restart_frame_rate_clock();

            /* Waits for the screen to want a frame. False if the wait timed out. */
            bool wait_for_screen();

        private:
            Stereo::Transport::FrameChannel* m_channel_ptr;
            int64_t                          m_last_frame_qpc; /* of the last frame that was held to a rate, or began one; 0: none yet */
        };
    }
}

#endif /* STEREO_CONVERTERDLL_HOST_MEMORY_EXPORTER_H */
