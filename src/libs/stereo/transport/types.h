/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_TRANSPORT_TYPES_H
#define STEREO_TRANSPORT_TYPES_H

#include "stereo/core/types.h"
#include <stdint.h>

namespace Stereo
{
    namespace Transport
    {
        /* How the pixels of an image get from the application to the program that shows them. Each kind is one bit, so what a side can do
         * is a mask (see FrameChannel::get_negotiated_kind()). The kinds are ordered by how much they save, and the last is the best. A
         * kind that shares a texture or a heap with the GPU of the program that shows the images would come after host memory.
         */
        enum class TransportKind : uint32_t
        {
            NONE        = 0,
            HOST_MEMORY = 1 /* the pixels are in the memory of the channel, written there by the CPU of the application */
        };

        typedef uint32_t TransportKindMask;

        /* The format of a pixel. */
        enum class PixelFormat : uint32_t
        {
            B8G8R8A8 = 0 /* 8 bits each of blue, green and red, followed by an unused byte */
        };

        /* The result of an export: whether the exporter passed the frame on. */
        enum class ExportStatus
        {
            EXPORTED,
            FAILED,   /* the frame cannot be passed on: the surface cannot be read, its format is not converted, or it does not fit */
            NOT_READY /* the other side has not said yet which kinds it takes: not a failure, the frame is left out                 */
        };

        /* An image that is passed from the application to the program that shows it. A pixel has the given format. The top row comes first,
         * and every row starts one pitch after the previous row. An image of two eyes holds the left eye and the right eye side by side,
         * each taking half of the width.
         */
        struct FrameInfo
        {
            uint64_t      frame_index  = 0; /* counted by the producer; a gap means that a frame was never seen                         */
            uint32_t      height       = 0;
            TransportKind kind         = TransportKind::HOST_MEMORY;
            uint32_t      n_eyes       = 1; /* 1 or 2                                                                                   */
            PixelFormat   pixel_format = PixelFormat::B8G8R8A8;
            uint32_t      pitch        = 0; /* bytes from one row to the next; set by the channel, not by the producer                  */
            double        timestamp_seconds = 0.0; /* time of the present call, counted from the producer's start; it names a moment of the run */
            uint32_t      width        = 0; /* of the whole image, both eyes included                                                   */
        };

        /* A frame that a consumer has taken from a channel, with what it needs to use it. It is valid until the consumer releases it. */
        struct ImportedFrame
        {
            FrameInfo      info;
            const uint8_t* pixel_ptr = nullptr; /* for the host memory kind */
        };

        /* What a producer gives an exporter to pass on. The pointers belong to the exporter's API, and each exporter defines what they are:
         * a device, and the surfaces of the two eyes. A null left surface means the surface that the exporter takes by default (the back
         * buffer). A null right surface means a frame of one eye.
         */
        struct ExportSource
        {
            void*                   api_device_ptr    = nullptr;
            void*                   left_surface_ptr  = nullptr;
            Stereo::Core::PixelRect picture;                      /* the picture in each eye image; the rest is bars. Empty: all */
            void*                   right_surface_ptr = nullptr;
            double                  timestamp_seconds = 0.0;      /* time of the present call, counted from the producer's start */
        };
    }
}

#endif /* STEREO_TRANSPORT_TYPES_H */
