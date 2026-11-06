/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Types of the images of replayed frames.
 */
#ifndef STEREO_REPLAY_FRAMEIMAGES_TYPES_H
#define STEREO_REPLAY_FRAMEIMAGES_TYPES_H

#include <stdint.h>
#include <vector>

namespace Stereo
{
    namespace Replay
    {
        namespace FrameImages
        {
            /* What of the two eyes of a frame an image shows. */
            enum class EyeView
            {
                ANAGLYPH, /* both, red-cyan                                */
                BOTH,     /* the left one and the right one, side by side  */
                LEFT,
                RIGHT
            };

            /* An image as 8-bit B, G, R and A (always 255) for each pixel, row after row, top row first. */
            struct Image
            {
                uint32_t             height = 0;
                std::vector<uint8_t> pixel_u8_vec;
                uint32_t             width  = 0;
            };

            /* How two images of one size differ. A pixel differs if any of its channels does. */
            struct ImageDifference
            {
                uint32_t bounding_box_max_x       = 0; /* of the pixels that differ, inclusive  */
                uint32_t bounding_box_max_y       = 0;
                uint32_t bounding_box_min_x       = 0;
                uint32_t bounding_box_min_y       = 0;
                bool     has_same_size            = true;
                uint8_t  max_channel_difference   = 0;
                uint64_t n_differing_pixels       = 0;
            };
        }
    }
}

#endif /* STEREO_REPLAY_FRAMEIMAGES_TYPES_H */
