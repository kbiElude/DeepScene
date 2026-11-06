/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "Stereo/Replay/FrameImages/image_comparer.h"
#include <algorithm>

namespace
{
    constexpr size_t N_BYTES_PER_PIXEL = 4; /* B, G, R, A */

    /* True if both images have the pixels their size says. */
    bool are_comparable(const Stereo::Replay::FrameImages::Image& in_first_image,
                        const Stereo::Replay::FrameImages::Image& in_second_image)
    {
        const size_t n_bytes = static_cast<size_t>(in_first_image.width) * in_first_image.height * N_BYTES_PER_PIXEL;

        return (in_first_image.width  == in_second_image.width        &&
                in_first_image.height == in_second_image.height       &&
                in_first_image.pixel_u8_vec.size()  >= n_bytes        &&
                in_second_image.pixel_u8_vec.size() >= n_bytes);
    }
}

Stereo::Replay::FrameImages::ImageDifference Stereo::Replay::FrameImages::ImageComparer::compare(const Image& in_first_image,
                                                                                                 const Image& in_second_image)
{
    ImageDifference result;

    if (are_comparable(in_first_image,
                       in_second_image) == false)
    {
        result.has_same_size = false;

        goto end;
    }

    for (uint32_t n_y = 0;
                  n_y < in_first_image.height;
                ++n_y)
    {
        for (uint32_t n_x = 0;
                      n_x < in_first_image.width;
                    ++n_x)
        {
            const size_t   offset       = (static_cast<size_t>(n_y) * in_first_image.width + n_x) * N_BYTES_PER_PIXEL;
            const uint8_t* first_ptr    = in_first_image.pixel_u8_vec.data()  + offset;
            const uint8_t* second_ptr   = in_second_image.pixel_u8_vec.data() + offset;
            uint8_t        max_distance = 0;

            for (size_t n_channel = 0;
                        n_channel < N_BYTES_PER_PIXEL;
                      ++n_channel)
            {
                const uint8_t distance = (first_ptr[n_channel] > second_ptr[n_channel]) ? static_cast<uint8_t>(first_ptr[n_channel]  - second_ptr[n_channel])
                                                                                        : static_cast<uint8_t>(second_ptr[n_channel] - first_ptr[n_channel]);

                max_distance = std::max(max_distance,
                                        distance);
            }

            if (max_distance == 0)
            {
                continue;
            }

            if (result.n_differing_pixels == 0)
            {
                result.bounding_box_max_x = n_x;
                result.bounding_box_max_y = n_y;
                result.bounding_box_min_x = n_x;
                result.bounding_box_min_y = n_y;
            }

            result.bounding_box_max_x     = std::max(result.bounding_box_max_x,
                                                     n_x);
            result.bounding_box_max_y     = std::max(result.bounding_box_max_y,
                                                     n_y);
            result.bounding_box_min_x     = std::min(result.bounding_box_min_x,
                                                     n_x);
            result.bounding_box_min_y     = std::min(result.bounding_box_min_y,
                                                     n_y);
            result.max_channel_difference = std::max(result.max_channel_difference,
                                                     max_distance);

            result.n_differing_pixels++;
        }
    }

end:
    return result;
}

bool Stereo::Replay::FrameImages::ImageComparer::make_difference_image(const Image& in_first_image,
                                                                       const Image& in_second_image,
                                                                       Image*       out_image_ptr)
{
    constexpr uint8_t OPAQUE_ALPHA = 255;
    constexpr uint8_t RED_CHANNEL  = 2;   /* the channels are B, G, R, A */

    size_t n_pixels = 0;
    bool   result   = false;

    *out_image_ptr = Image();

    if (are_comparable(in_first_image,
                       in_second_image) == false)
    {
        goto end;
    }

    n_pixels = static_cast<size_t>(in_first_image.width) * in_first_image.height;

    out_image_ptr->height = in_first_image.height;
    out_image_ptr->width  = in_first_image.width;

    out_image_ptr->pixel_u8_vec.assign(n_pixels * N_BYTES_PER_PIXEL,
                                       0);

    for (size_t n_pixel = 0;
                n_pixel < n_pixels;
              ++n_pixel)
    {
        const size_t   offset     = n_pixel * N_BYTES_PER_PIXEL;
        const uint8_t* first_ptr  = in_first_image.pixel_u8_vec.data()  + offset;
        const uint8_t* second_ptr = in_second_image.pixel_u8_vec.data() + offset;
        uint8_t* const out_ptr    = out_image_ptr->pixel_u8_vec.data()  + offset;

        out_ptr[N_BYTES_PER_PIXEL - 1] = OPAQUE_ALPHA;

        for (size_t n_channel = 0;
                    n_channel < N_BYTES_PER_PIXEL;
                  ++n_channel)
        {
            if (first_ptr[n_channel] != second_ptr[n_channel])
            {
                out_ptr[RED_CHANNEL] = OPAQUE_ALPHA;

                break;
            }
        }
    }

    result = true;
end:
    return result;
}
