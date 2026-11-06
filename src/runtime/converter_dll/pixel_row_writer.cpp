/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "interception/hooks/D3D8/types.h"
#include "pixel_converter.h"
#include "pixel_row_writer.h"
#include <string.h>

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

    /* The formats of 32 bits are as they are wanted; the others are converted first. */
    if (in_format != Interception::Hooks::D3D8::D3DFMT_A8R8G8B8 &&
        in_format != Interception::Hooks::D3D8::D3DFMT_X8R8G8B8)
    {
        is_ok = ::Replay::ReplayEngineDLL::PixelConverter::convert_to_bgra8(in_format,
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
