/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_TRANSPORT_I_FRAME_EXPORTER_H
#define STEREO_TRANSPORT_I_FRAME_EXPORTER_H

#include "stereo/transport/types.h"
#include <string>

namespace Stereo
{
    namespace Transport
    {
        /* The producer's end of a channel, for one graphics API. It takes the eye images that an application of that API has drawn and
         * gets them to the program that shows them, in whichever way the API allows. An API that cannot share its surfaces with the GPU of
         * that program (Direct3D 8) offers only the host memory kind. An API that can share them offers a sharing kind, and also the host
         * memory kind in case the program cannot take the sharing kind. What the program takes is recorded in the channel (see
         * FrameChannel::get_negotiated_kind()).
         */
        class IFrameExporter
        {
        public:
            virtual ~IFrameExporter() = default;

            /* Passes the frame on, using the kind that the two sides have agreed on. The caller counts the frame index. */
            virtual ExportStatus export_frame(const uint64_t&     in_frame_index,
                                              const ExportSource& in_source) = 0;

            /* The kinds of transport that the exporter can use to make a frame available. */
            virtual TransportKindMask get_offered_kinds() const = 0;

            /* Returns the cost of passing frames since the last call, as text for the log, and starts counting again. The default returns an
             * empty string.
             */
            virtual std::string take_timing_text()
            {
                return std::string();
            }

            /* Waits until the screen wants a frame, if the program that shows the frames says when (see FrameChannel::wait_for_pace()). The
             * application's thread calls it after a frame is presented, so that the application makes the next frame when it is wanted.
             */
            virtual void wait_for_pace()
            {
                /* Stub */
            }
        };
    }
}

#endif /* STEREO_TRANSPORT_I_FRAME_EXPORTER_H */
