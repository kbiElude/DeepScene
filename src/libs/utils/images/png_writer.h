/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Writes an image as a PNG with 8 bits per channel in RGB. The alpha channel is dropped because it is always opaque. Compression is done by miniz.
 */
#ifndef UTILS_IMAGES_PNG_WRITER_H
#define UTILS_IMAGES_PNG_WRITER_H

#include "utils/images/types.h"
#include <stdint.h>
#include <string>
#include <vector>

namespace Utils
{
    namespace Images
    {
        class PngWriter
        {
        public:
            /* Returns the bytes of the file. Returns false if the image is empty or has fewer pixels than its size says. */
            static bool encode(const Image&          in_image,
                               std::vector<uint8_t>* out_file_u8_vec_ptr);

            /* Returns false if the image cannot be encoded or the file cannot be written. */
            static bool write_file(const std::string& in_path,
                                   const Image&       in_image);
        };
    }
}

#endif /* UTILS_IMAGES_PNG_WRITER_H */
