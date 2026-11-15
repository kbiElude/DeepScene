/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_TRANSPORT_I_FRAME_IMPORTER_H
#define STEREO_TRANSPORT_I_FRAME_IMPORTER_H

#include "stereo/transport/types.h"

namespace Stereo
{
    namespace Transport
    {
        /* The consumer's end of a channel, for one kind of transport. It makes the newest frame available to whatever shows it (see
         * ImportedFrame). A frame is valid until it is released.
         */
        class IFrameImporter
        {
        public:
            virtual ~IFrameImporter() = default;

            /* Returns the newest frame in out_frame_ptr. Returns false if there is none yet. */
            virtual bool acquire(ImportedFrame* out_frame_ptr) = 0;

            /* Call it when the acquired frame is no longer needed. */
            virtual void release() = 0;

            /* set_pace_delay_us() sets how long, in microseconds, the producer waits after a signal. signal_pace() tells the producer that the
             * screen is about to want a frame (see FrameChannel::signal_pace()). Both do nothing for a transport that has no producer to hold
             * back.
             */
            virtual void set_pace_delay_us(const uint32_t& in_delay_us)
            {
                (void) in_delay_us;
            }

            virtual void signal_pace()
            {
                /* Stub */
            }

            /* Sets the ratio of the viewer's eye distance to the eye distance that the producer's separation is made for (see
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
