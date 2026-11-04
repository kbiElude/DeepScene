/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "Stereo/Replay/FrameImages/image_composer.h"
#include <algorithm>
#include <string.h>

namespace
{
    constexpr size_t  N_BYTES_PER_PIXEL = 4; /* B, G, R, A */
    constexpr uint8_t OPAQUE_ALPHA      = 255;

    /* True if the image has the pixels its size says, and some. */
    bool is_usable(const Stereo::Replay::FrameImages::Image& in_image)
    {
        return (in_image.width  != 0                                                                                      &&
                in_image.height != 0                                                                                      &&
                in_image.pixel_u8_vec.size() >= static_cast<size_t>(in_image.width) * in_image.height * N_BYTES_PER_PIXEL);
    }
}

bool Stereo::Replay::FrameImages::ImageComposer::make_anaglyph(const Image& in_left_image,
                                                               const Image& in_right_image,
                                                               Image*       out_image_ptr)
{
    constexpr size_t RED_CHANNEL = 2; /* the channels are B, G, R, A */

    size_t n_pixels = 0;
    bool   result   = false;

    *out_image_ptr = Image();

    if (is_usable(in_left_image)                    == false ||
        is_usable(in_right_image)                   == false ||
        in_left_image.width                         != in_right_image.width ||
        in_left_image.height                        != in_right_image.height)
    {
        goto end;
    }

    n_pixels = static_cast<size_t>(in_left_image.width) * in_left_image.height;

    out_image_ptr->height       = in_left_image.height;
    out_image_ptr->pixel_u8_vec = in_right_image.pixel_u8_vec;
    out_image_ptr->width        = in_left_image.width;

    out_image_ptr->pixel_u8_vec.resize(n_pixels * N_BYTES_PER_PIXEL);

    for (size_t n_pixel = 0;
                n_pixel < n_pixels;
              ++n_pixel)
    {
        const size_t offset = n_pixel * N_BYTES_PER_PIXEL;

        out_image_ptr->pixel_u8_vec.at(offset + RED_CHANNEL) = in_left_image.pixel_u8_vec.at(offset + RED_CHANNEL);
    }

    result = true;
end:
    return result;
}

bool Stereo::Replay::FrameImages::ImageComposer::make_contact_sheet(const std::vector<Image>& in_image_vec,
                                                                    const uint32_t&           in_n_columns,
                                                                    Image*                    out_image_ptr)
{
    constexpr uint32_t GAP_N_PIXELS = 4;
    constexpr uint8_t  BACKGROUND   = 40;

    uint32_t cell_height = 0;
    uint32_t cell_width  = 0;
    uint32_t n_rows      = 0;
    bool     result      = false;

    *out_image_ptr = Image();

    if (in_image_vec.empty() ||
        in_n_columns == 0)
    {
        goto end;
    }

    for (const auto& image : in_image_vec)
    {
        if (is_usable(image) == false)
        {
            goto end;
        }

        cell_height = std::max(cell_height,
                               image.height);
        cell_width  = std::max(cell_width,
                               image.width);
    }

    n_rows = static_cast<uint32_t>((in_image_vec.size() + in_n_columns - 1) / in_n_columns);

    out_image_ptr->height = n_rows       * cell_height + (n_rows       - 1) * GAP_N_PIXELS;
    out_image_ptr->width  = in_n_columns * cell_width  + (in_n_columns - 1) * GAP_N_PIXELS;

    out_image_ptr->pixel_u8_vec.assign(static_cast<size_t>(out_image_ptr->width) * out_image_ptr->height * N_BYTES_PER_PIXEL,
                                       BACKGROUND);

    for (size_t n_pixel = 0;
                n_pixel < static_cast<size_t>(out_image_ptr->width) * out_image_ptr->height;
              ++n_pixel)
    {
        out_image_ptr->pixel_u8_vec.at(n_pixel * N_BYTES_PER_PIXEL + N_BYTES_PER_PIXEL - 1) = OPAQUE_ALPHA;
    }

    for (size_t n_image = 0;
                n_image < in_image_vec.size();
              ++n_image)
    {
        const Image&   image  = in_image_vec.at(n_image);
        const uint32_t origin_x = static_cast<uint32_t>(n_image % in_n_columns) * (cell_width  + GAP_N_PIXELS);
        const uint32_t origin_y = static_cast<uint32_t>(n_image / in_n_columns) * (cell_height + GAP_N_PIXELS);

        for (uint32_t n_y = 0;
                      n_y < image.height;
                    ++n_y)
        {
            ::memcpy(out_image_ptr->pixel_u8_vec.data() + (static_cast<size_t>(origin_y + n_y) * out_image_ptr->width + origin_x) * N_BYTES_PER_PIXEL,
                     image.pixel_u8_vec.data()          +  static_cast<size_t>(n_y)            * image.width                      * N_BYTES_PER_PIXEL,
                     static_cast<size_t>(image.width) * N_BYTES_PER_PIXEL);
        }
    }

    result = true;
end:
    return result;
}
