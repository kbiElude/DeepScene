/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_TRANSPORT_I_FRAME_EXPORTER_H
#define STEREO_TRANSPORT_I_FRAME_EXPORTER_H

#include "Stereo/Transport/types.h"
#include <string>

namespace Stereo
{
    namespace Transport
    {
        /* The producer's end of a channel, for one graphics API: it takes the images of the eyes that an application of the API has
         * drawn, and gets them to the program which shows them, whichever way that API can. An API which cannot share its surfaces
         * with the GPU of that program (Direct3D 8) offers only the host memory kind; one which can offers a kind that shares them,
         * and the host memory kind if the program cannot take that. What the program takes is in the channel (see
         * FrameChannel::get_negotiated_kind()).
         */
        class IFrameExporter
        {
        public:
            virtual ~IFrameExporter() = default;

            /* Passes the frame on, by the kind the two sides have agreed on. The frame index is counted by the caller. */
            virtual ExportStatus export_frame(const uint64_t&     in_frame_index,
                                              const ExportSource& in_source) = 0;

            /* The kinds of transport the exporter can make a frame available by. */
            virtual TransportKindMask get_offered_kinds() const = 0;

            /* What the passing of frames cost since the last time this was called, as text for the log, and starts counting again; none, by default. */
            virtual std::string take_timing_text()
            {
                return std::string();
            }

            /* Waits for the screen to want a frame, if the program which shows them says when (see FrameChannel::wait_for_pace()); called by the
             * application's thread after a frame is presented, so that the application makes the next one when it is wanted.
             */
            virtual void wait_for_pace()
            {
                /* Stub */
            }
        };
    }
}

#endif /* STEREO_TRANSPORT_I_FRAME_EXPORTER_H */
