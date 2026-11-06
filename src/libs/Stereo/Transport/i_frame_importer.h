/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_TRANSPORT_I_FRAME_IMPORTER_H
#define STEREO_TRANSPORT_I_FRAME_IMPORTER_H

#include "Stereo/Transport/types.h"

namespace Stereo
{
    namespace Transport
    {
        /* The consumer's end of a channel, for one kind of transport: it makes the newest frame available to what shows it (see
         * ImportedFrame). A frame is good until it is released.
         */
        class IFrameImporter
        {
        public:
            virtual ~IFrameImporter() = default;

            /* The newest frame. Returns false if there is none yet. */
            virtual bool acquire(ImportedFrame* out_frame_ptr) = 0;

            /* The frame which was acquired is not needed any more. */
            virtual void release() = 0;

            /* The screen is about to want a frame (see FrameChannel::signal_pace()); and the delay that the producer is to wait after it, in
             * microseconds. Of no use to a transport that has no producer to hold back.
             */
            virtual void set_pace_delay_us(const uint32_t& in_delay_us)
            {
                (void) in_delay_us;
            }

            virtual void signal_pace()
            {
                /* Stub */
            }

            /* How the distance between the eyes of the person who looks compares with the one the producer's separation is made for (see
             * FrameChannel::get_ipd_scale()).
             */
            virtual void set_ipd_scale(const float& in_ipd_scale)
            {
                (void) in_ipd_scale;
            }
        };
    }
}

#endif /* STEREO_TRANSPORT_I_FRAME_IMPORTER_H */
