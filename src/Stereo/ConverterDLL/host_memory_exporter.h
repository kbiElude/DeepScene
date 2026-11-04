/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_CONVERTERDLL_HOST_MEMORY_EXPORTER_H
#define STEREO_CONVERTERDLL_HOST_MEMORY_EXPORTER_H

#include "Stereo/Transport/frame_channel.h"
#include "Stereo/Transport/i_frame_exporter.h"

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

            /* False until the other side has said that it takes the host memory kind. */
            bool is_host_memory_negotiated() const;

            /* Waits for the screen to want a frame. False if the wait timed out. */
            bool wait_for_screen();

        private:
            Stereo::Transport::FrameChannel* m_channel_ptr;
        };
    }
}

#endif /* STEREO_CONVERTERDLL_HOST_MEMORY_EXPORTER_H */
