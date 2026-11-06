/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_TRANSPORT_HOST_MEMORY_IMPORTER_H
#define STEREO_TRANSPORT_HOST_MEMORY_IMPORTER_H

#include "stereo/transport/frame_channel.h"
#include "stereo/transport/i_frame_importer.h"
#include <memory>

namespace Stereo
{
    namespace Transport
    {
        /* Takes the frames that the producer put in the memory of the channel. It says, on the channel, that this is what it takes. */
        class HostMemoryImporter : public IFrameImporter
        {
        public:
            explicit HostMemoryImporter(std::unique_ptr<FrameChannel> in_channel_ptr);

            bool acquire          (ImportedFrame*  out_frame_ptr) override;
            void release          ()                              override;
            void set_ipd_scale    (const float&    in_ipd_scale)  override;
            void set_pace_delay_us(const uint32_t& in_delay_us)   override;
            void signal_pace      ()                              override;

        private:
            std::unique_ptr<FrameChannel> m_channel_ptr;
        };
    }
}

#endif /* STEREO_TRANSPORT_HOST_MEMORY_IMPORTER_H */
