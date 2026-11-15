/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Writes images as an animated GIF that loops forever. A GIF frame has at most 256 colours, so each frame gets its own palette, built by median cut over the
 * frame's colours (reduced to 5 bits per channel). There is no dithering: a colour with no other colour near it, such as the magenta of a highlight, stays as it is.
 */
#ifndef UTILS_IMAGES_GIF_WRITER_H
#define UTILS_IMAGES_GIF_WRITER_H

#include "utils/images/types.h"
#include <array>
#include <stdint.h>
#include <string>
#include <vector>

namespace Utils
{
    namespace Images
    {
        class GifWriter
        {
        public:
            /* Returns the bytes of the file. Returns false if there is no frame, a frame is empty or has fewer pixels than its size says, the frames differ
             * in size, or they are larger than a GIF allows (65535 pixels in each direction).
             */
            static bool encode(const std::vector<Image>& in_frame_vec,
                               const uint32_t&           in_frame_delay_n_milliseconds,
                               std::vector<uint8_t>*     out_file_u8_vec_ptr);

            /* Returns false if the frames cannot be encoded or the file cannot be written. */
            static bool write_file(const std::string&        in_path,
                                   const std::vector<Image>& in_frame_vec,
                                   const uint32_t&           in_frame_delay_n_milliseconds);

        private:
            /* Writes a 16-bit value, low byte first. */
            static void append_u16(const uint32_t&       in_value,
                                   std::vector<uint8_t>* inout_byte_vec_ptr);

            /* Encodes the colour indices of a frame as GIF stores them: the minimum code size, the blocks of compressed codes, and the terminator. */
            static void compress_lzw(const std::vector<uint8_t>& in_index_u8_vec,
                                     std::vector<uint8_t>*       inout_file_u8_vec_ptr);

            /* Builds the palette of a frame (R, G, B for 256 colours; unused entries are black) and the palette index of each pixel. */
            static void quantize(const Image&              in_image,
                                 std::array<uint8_t, 768>* out_palette_u8vec768_ptr,
                                 std::vector<uint8_t>*     out_index_u8_vec_ptr);
        };
    }
}

#endif /* UTILS_IMAGES_GIF_WRITER_H */
