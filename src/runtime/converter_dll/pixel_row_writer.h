/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_CONVERTERDLL_PIXEL_ROW_WRITER_H
#define STEREO_CONVERTERDLL_PIXEL_ROW_WRITER_H

#include <stdint.h>
#include <vector>

namespace Stereo
{
    namespace ConverterDLL
    {
        /* Copies the rows of the image of one eye into an image that holds one or two eyes side by side. Each pixel has 8 bits for each of B, G, R and A
         * (see PixelConverter). This is the format of the frames of the channel. Every exporter uses it to write the surface of an eye, whatever API the
         * surface comes from.
         */
        class PixelRowWriter
        {
        public:
            /* in_format is a Direct3D 8 format (D3DFORMAT). The formats of the other APIs are converted to it (see PixelConverter). A 32-bit format
             * that already has the wanted layout is copied as it is. Any other format is converted first, into inout_scratch_ptr. The caller keeps that
             * vector so that its memory is reused. in_eye is the index of the eye in the destination image. The rows of the eye start at the pixel
             * column in_width * in_eye. Returns false and writes nothing if the format cannot be converted.
             */
            static bool write_eye(const uint32_t&       in_format,
                                  const uint32_t&       in_width,
                                  const uint32_t&       in_height,
                                  const uint8_t*        in_source_ptr,
                                  const uint32_t&       in_source_pitch,
                                  const uint32_t&       in_eye,
                                  uint8_t*              out_destination_ptr,
                                  const uint32_t&       in_destination_pitch,
                                  std::vector<uint8_t>* inout_scratch_ptr);
        };
    }
}

#endif /* STEREO_CONVERTERDLL_PIXEL_ROW_WRITER_H */
