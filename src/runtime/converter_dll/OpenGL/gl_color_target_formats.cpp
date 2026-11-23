/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "gl_color_target_formats.h"
#include <array>
#include <windows.h>

bool Stereo::ConverterDLL::GLColorTargetFormats::find_internal_format(const GLColorLayout& in_layout,
                                                                      uint32_t*            out_internal_format_ptr)
{
    constexpr uint32_t RGB565 = 0x8D62;
    constexpr uint32_t RGB8   = 0x8051;
    constexpr uint32_t RGBA8  = 0x8058;

    /* Every format that has a render target of exactly its channels. */
    struct Entry
    {
        uint32_t alpha_bits;
        uint32_t blue_bits;
        uint32_t green_bits;
        uint32_t internal_format;
        uint32_t red_bits;
    };

    constexpr std::array<Entry, 3> ENTRY_ARRAY =
    {{
        {0, 5, 6, RGB565, 5},
        {0, 8, 8, RGB8,   8},
        {8, 8, 8, RGBA8,  8}
    }};

    bool result = false;

    for (const auto& entry : ENTRY_ARRAY)
    {
        if (in_layout.is_rgba                       &&
            in_layout.alpha_bits == entry.alpha_bits &&
            in_layout.blue_bits  == entry.blue_bits  &&
            in_layout.green_bits == entry.green_bits &&
            in_layout.red_bits   == entry.red_bits)
        {
            *out_internal_format_ptr = entry.internal_format;
            result                   = true;

            break;
        }
    }

    return result;
}

bool Stereo::ConverterDLL::GLColorTargetFormats::read_layout(const void*    in_device_ptr,
                                                             GLColorLayout* out_layout_ptr)
{
    const HDC device   = static_cast<HDC>(const_cast<void*>(in_device_ptr) );
    const int n_format = ::GetPixelFormat(device);
    bool      result   = false;

    if (n_format != 0)
    {
        PIXELFORMATDESCRIPTOR description = {};

        result = (::DescribePixelFormat(device,
                                        n_format,
                                        sizeof(description),
                                       &description) != 0);

        if (result)
        {
            out_layout_ptr->alpha_bits = description.cAlphaBits;
            out_layout_ptr->blue_bits  = description.cBlueBits;
            out_layout_ptr->green_bits = description.cGreenBits;
            out_layout_ptr->is_rgba    = (description.iPixelType == PFD_TYPE_RGBA);
            out_layout_ptr->red_bits   = description.cRedBits;
        }
    }

    return result;
}
