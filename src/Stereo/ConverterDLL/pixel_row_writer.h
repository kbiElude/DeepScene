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
        /* Puts the rows of the image of an eye into an image of one or two eyes, side by side, as pixels of 8 bits each of B, G, R and A (see
         * PixelConverter), which is what the frames of the channel are made of. It is what each exporter does with the surface of an eye, whatever
         * the API it was read from is.
         */
        class PixelRowWriter
        {
        public:
            /* The format is a Direct3D 8 format (D3DFORMAT), which the formats of the other APIs are given as (see PixelConverter). The pixels of
             * 32 bits that are as they are wanted are copied as they are; the others are converted first, in the scratch vector, which is kept by
             * the caller so that its room is used again. The eye is the index of the eye in the image, which is where its rows begin, in pixels,
             * across: width times the index. Returns false, and writes nothing, for a format that is not converted.
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
