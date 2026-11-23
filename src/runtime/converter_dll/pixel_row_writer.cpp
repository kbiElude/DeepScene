/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "interception/hooks/D3D8/types.h"
#include "d3d_format_bgra8_converter.h"
#include "pixel_row_writer.h"
#include <string.h>

void Stereo::ConverterDLL::PixelRowWriter::blank_bars(const Stereo::Core::PixelRect& in_picture,
                                                     const uint32_t&                in_width,
                                                     const uint32_t&                in_height,
                                                     const uint32_t&                in_eye,
                                                     uint8_t*                       inout_destination_ptr,
                                                     const uint32_t&                in_destination_pitch)
{
    constexpr uint32_t BLACK_BGRA = 0xFF000000;

    const bool is_whole_image = (in_picture.x      == 0        &&
                                 in_picture.y      == 0        &&
                                 in_picture.width  == in_width &&
                                 in_picture.height == in_height);

    if (is_whole_image == false &&
        in_picture.width  != 0  &&
        in_picture.height != 0)
    {
        const uint32_t picture_right  = in_picture.x + in_picture.width;
        const uint32_t picture_bottom = in_picture.y + in_picture.height;

        for (uint32_t n_row = 0;
                      n_row < in_height;
                    ++n_row)
        {
            uint32_t* const row_ptr = reinterpret_cast<uint32_t*>(inout_destination_ptr + static_cast<size_t>(n_row) * in_destination_pitch) + static_cast<size_t>(in_eye) * in_width;
            const bool      is_bar  = (n_row < in_picture.y ||
                                       n_row >= picture_bottom);

            for (uint32_t n_column = 0;
                          n_column < in_width;
                        ++n_column)
            {
                if (is_bar                    ||
                    n_column < in_picture.x   ||
                    n_column >= picture_right)
                {
                    row_ptr[n_column] = BLACK_BGRA;
                }
            }
        }
    }
}

void Stereo::ConverterDLL::PixelRowWriter::make_opaque(const uint32_t& in_width,
                                                      const uint32_t& in_height,
                                                      const uint32_t& in_eye,
                                                      uint8_t*        inout_destination_ptr,
                                                      const uint32_t& in_destination_pitch)
{
    constexpr uint32_t OPAQUE_ALPHA_MASK = 0xFF000000;

    for (uint32_t n_row = 0;
                  n_row < in_height;
                ++n_row)
    {
        uint32_t* const row_ptr = reinterpret_cast<uint32_t*>(inout_destination_ptr + static_cast<size_t>(n_row) * in_destination_pitch) + static_cast<size_t>(in_eye) * in_width;

        for (uint32_t n_column = 0;
                      n_column < in_width;
                    ++n_column)
        {
            row_ptr[n_column] |= OPAQUE_ALPHA_MASK;
        }
    }
}

bool Stereo::ConverterDLL::PixelRowWriter::write_eye(const uint32_t&       in_format,
                                                     const uint32_t&       in_width,
                                                     const uint32_t&       in_height,
                                                     const uint8_t*        in_source_ptr,
                                                     const uint32_t&       in_source_pitch,
                                                     const uint32_t&       in_eye,
                                                     uint8_t*              out_destination_ptr,
                                                     const uint32_t&       in_destination_pitch,
                                                     std::vector<uint8_t>* inout_scratch_ptr)
{
    constexpr uint32_t BYTES_PER_PIXEL = sizeof(uint32_t);

    bool           is_ok        = true;
    const uint8_t* source_ptr   = in_source_ptr;
    uint32_t       source_pitch = in_source_pitch;

    /* The 32-bit formats that already have the wanted layout are copied as they are. All other formats are converted first. */
    if (in_format != Interception::Hooks::D3D8::D3DFMT_A8R8G8B8 &&
        in_format != Interception::Hooks::D3D8::D3DFMT_X8R8G8B8)
    {
        is_ok = ::Replay::ReplayEngineDLL::D3DFormatBgra8Converter::convert_to_bgra8(in_format,
                                                                                     in_width,
                                                                                     in_height,
                                                                                     in_source_pitch,
                                                                                     in_source_ptr,
                                                                                     inout_scratch_ptr);

        source_ptr   = inout_scratch_ptr->data();
        source_pitch = in_width * BYTES_PER_PIXEL;
    }

    for (uint32_t n_row = 0;
                  n_row < in_height && is_ok;
                ++n_row)
    {
        ::memcpy(out_destination_ptr + static_cast<size_t>(n_row) * in_destination_pitch + static_cast<size_t>(in_eye) * in_width * BYTES_PER_PIXEL,
                 source_ptr          + static_cast<size_t>(n_row) * source_pitch,
                 static_cast<size_t>(in_width) * BYTES_PER_PIXEL);
    }

    return is_ok;
}
