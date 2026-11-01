/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREOTRANSPORT_TYPES_H
#define STEREOTRANSPORT_TYPES_H

#include <stdint.h>

namespace Stereo
{
    namespace StereoTransport
    {
        /* How the pixels of an image get from the application to the program which shows them. A kind is a bit, so that what a side can
         * do is a mask (see FrameChannel::get_negotiated_kind()). More kinds will come with the APIs which can share a texture or a heap
         * with the GPU of the program which shows the images; the kinds are in the order of how much they save, the last being the best.
         */
        enum class TransportKind : uint32_t
        {
            NONE        = 0,
            HOST_MEMORY = 1 /* the pixels are in the memory of the channel, put there by the CPU of the application */
        };

        typedef uint32_t TransportKindMask;

        /* What a pixel is. */
        enum class PixelFormat : uint32_t
        {
            B8G8R8A8 = 0 /* 8 bits each of blue, green, red and a byte which is not used */
        };

        /* How an exporter says that it did, or did not, pass a frame on. */
        enum class ExportStatus
        {
            EXPORTED,
            FAILED,   /* the frame cannot be passed on: the surface cannot be read, its format is not converted, it does not fit */
            NOT_READY /* the other side has not said what it takes yet: not a failure, the frame is left out                  */
        };

        /* An image which is passed from the application to the program which shows it. A pixel is as the format says, the top row first,
         * every row starting a pitch after the one before. An image of two eyes is the left one and the right one side by side, each half
         * of the width.
         */
        struct FrameInfo
        {
            uint64_t      frame_index  = 0; /* counted by the producer; a gap is a frame which was never seen                          */
            uint32_t      height       = 0;
            TransportKind kind         = TransportKind::HOST_MEMORY;
            uint32_t      n_eyes       = 1; /* 1 or 2                                                                                 */
            PixelFormat   pixel_format = PixelFormat::B8G8R8A8;
            uint32_t      pitch        = 0; /* bytes from a row to the next; set by the channel, not by the producer                  */
            double        timestamp_seconds = 0.0; /* of the present, since the producer started; to name a moment of the run           */
            uint32_t      width        = 0; /* of the whole image                                                                     */
        };

        /* A frame which a consumer has taken from a channel, and what it needs to use it. It is good until the consumer releases it. */
        struct ImportedFrame
        {
            FrameInfo      info;
            const uint8_t* pixel_ptr = nullptr; /* for the host memory kind */
        };

        /* What a producer gives an exporter to pass on. The pointers are of the API of the exporter, and what it says they are is for
         * each exporter to say: a device, and the surfaces of the two eyes. No left surface is the one the exporter takes by default
         * (the back buffer); no right surface is a frame of one eye.
         */
        struct ExportSource
        {
            void* api_device_ptr     = nullptr;
            void* left_surface_ptr   = nullptr;
            void* right_surface_ptr  = nullptr;
            double timestamp_seconds = 0.0; /* of the present, since the producer started */
        };
    }
}

#endif /* STEREOTRANSPORT_TYPES_H */
