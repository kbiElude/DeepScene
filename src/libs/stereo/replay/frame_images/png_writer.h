/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Writes an image as a PNG (8 bits per channel, RGB: the alpha of an image is always opaque), compressed by miniz.
 */
#ifndef STEREO_REPLAY_FRAMEIMAGES_PNG_WRITER_H
#define STEREO_REPLAY_FRAMEIMAGES_PNG_WRITER_H

#include "stereo/replay/frame_images/types.h"
#include <stdint.h>
#include <string>
#include <vector>

namespace Stereo
{
    namespace Replay
    {
        namespace FrameImages
        {
            class PngWriter
            {
            public:
                /* The bytes of the file. False if the image is empty or its pixels are fewer than its size says. */
                static bool encode(const Image&          in_image,
                                   std::vector<uint8_t>* out_file_u8_vec_ptr);

                /* False if the image cannot be encoded or the file cannot be written. */
                static bool write_file(const std::string& in_path,
                                       const Image&       in_image);
            };
        }
    }
}

#endif /* STEREO_REPLAY_FRAMEIMAGES_PNG_WRITER_H */
