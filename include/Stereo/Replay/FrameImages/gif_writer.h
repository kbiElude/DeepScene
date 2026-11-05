/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Writes images as an animated GIF that plays for ever. A GIF has at most 256 colours for a frame, so each frame has a palette of its own, made by median cut
 * of the colours it has (to 5 bits of each channel), with no dithering: a colour which no other is near, such as the magenta of a highlight, stays as it is.
 */
#ifndef STEREO_REPLAY_FRAMEIMAGES_GIF_WRITER_H
#define STEREO_REPLAY_FRAMEIMAGES_GIF_WRITER_H

#include "Stereo/Replay/FrameImages/types.h"
#include <array>
#include <stdint.h>
#include <string>
#include <vector>

namespace Stereo
{
    namespace Replay
    {
        namespace FrameImages
        {
            class GifWriter
            {
            public:
                /* The bytes of the file. False if there is no frame, a frame is empty or has fewer pixels than its size says, the frames are not of one size, or they are
                 * larger than a GIF can be (65535 pixels each way).
                 */
                static bool encode(const std::vector<Image>& in_frame_vec,
                                   const uint32_t&           in_frame_delay_n_milliseconds,
                                   std::vector<uint8_t>*     out_file_u8_vec_ptr);

                /* False if the frames cannot be encoded or the file cannot be written. */
                static bool write_file(const std::string&        in_path,
                                       const std::vector<Image>& in_frame_vec,
                                       const uint32_t&           in_frame_delay_n_milliseconds);

            private:
                /* The bytes of the colour indices of a frame as a GIF holds them: the smallest code size, the blocks of the compressed codes, and the end of them. */
                static void compress_lzw(const std::vector<uint8_t>& in_index_u8_vec,
                                         std::vector<uint8_t>*       inout_file_u8_vec_ptr);

                /* The palette of a frame (R, G, B of 256 colours; the ones left over are black) and the index of the palette that each pixel has. */
                static void quantize(const Image&                in_image,
                                     std::array<uint8_t, 768>*   out_palette_u8vec768_ptr,
                                     std::vector<uint8_t>*       out_index_u8_vec_ptr);
            };
        }
    }
}

#endif /* STEREO_REPLAY_FRAMEIMAGES_GIF_WRITER_H */
