/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "Interception/Hooks/D3D8/types.h"
#include "pixel_converter.h"
#include <string.h>

bool Replay::StereoReplayEngineDLL::PixelConverter::convert_to_bgra8(const uint32_t&       in_format,
                                                                     const uint32_t&       in_width,
                                                                     const uint32_t&       in_height,
                                                                     const uint32_t&       in_pitch,
                                                                     const uint8_t*        in_pixel_ptr,
                                                                     std::vector<uint8_t>* out_pixel_u8_vec_ptr)
{
    constexpr uint32_t N_BYTES_PER_OUTPUT_PIXEL = sizeof(uint32_t);
    constexpr uint32_t OPAQUE_ALPHA_MASK        = 0xff000000;

    const PixelLayout* layout_ptr = find_layout_ptr(in_format);

    if (layout_ptr == nullptr)
    {
        out_pixel_u8_vec_ptr->clear();

        goto end;
    }

    out_pixel_u8_vec_ptr->resize(static_cast<size_t>(in_width) * in_height * N_BYTES_PER_OUTPUT_PIXEL);

    for (uint32_t n_row = 0;
                  n_row < in_height;
                ++n_row)
    {
        const uint8_t*       in_row_ptr  = in_pixel_ptr + static_cast<size_t>(n_row) * in_pitch;
        uint8_t*             out_row_ptr = out_pixel_u8_vec_ptr->data() + static_cast<size_t>(n_row) * in_width * N_BYTES_PER_OUTPUT_PIXEL;

        if (layout_ptr->n_bytes == N_BYTES_PER_OUTPUT_PIXEL)
        {
            /* The 32-bit formats have the channels of the output; only the alpha is made opaque. */
            for (uint32_t n_column = 0;
                          n_column < in_width;
                        ++n_column)
            {
                uint32_t pixel = 0;

                ::memcpy(&pixel,
                         in_row_ptr + static_cast<size_t>(n_column) * N_BYTES_PER_OUTPUT_PIXEL,
                         sizeof(pixel) );

                pixel |= OPAQUE_ALPHA_MASK;

                ::memcpy(out_row_ptr + static_cast<size_t>(n_column) * N_BYTES_PER_OUTPUT_PIXEL,
                        &pixel,
                         sizeof(pixel) );
            }
        }
        else
        {
            const auto& lookup_vec = get_lookup_vec(*layout_ptr);

            for (uint32_t n_column = 0;
                          n_column < in_width;
                        ++n_column)
            {
                uint16_t value = 0;

                ::memcpy(&value,
                         in_row_ptr + static_cast<size_t>(n_column) * sizeof(value),
                         sizeof(value) );

                ::memcpy(out_row_ptr + static_cast<size_t>(n_column) * N_BYTES_PER_OUTPUT_PIXEL,
                        &lookup_vec[value],
                         sizeof(uint32_t) );
            }
        }
    }

end:
    return (layout_ptr != nullptr);
}

const Replay::StereoReplayEngineDLL::PixelConverter::PixelLayout* Replay::StereoReplayEngineDLL::PixelConverter::find_layout_ptr(const uint32_t& in_format)
{
    const PixelLayout* result_ptr = nullptr;

    for (const auto& layout : get_layout_vec() )
    {
        if (static_cast<uint32_t>(layout.format) == in_format)
        {
            result_ptr = &layout;
        }
    }

    return result_ptr;
}

uint32_t Replay::StereoReplayEngineDLL::PixelConverter::get_bytes_per_pixel(const uint32_t& in_format)
{
    const PixelLayout* layout_ptr = find_layout_ptr(in_format);

    return (layout_ptr != nullptr) ? layout_ptr->n_bytes
                                   : 0;
}

const std::vector<Replay::StereoReplayEngineDLL::PixelConverter::PixelLayout>& Replay::StereoReplayEngineDLL::PixelConverter::get_layout_vec()
{
    static const std::vector<PixelLayout> result_vec =
    {
        {{8, 0}, Interception::Hooks::D3D8::D3DFMT_A8R8G8B8, {8, 8}, 4, {8, 16}},
        {{4, 0}, Interception::Hooks::D3D8::D3DFMT_A4R4G4B4, {4, 4}, 2, {4, 8}},
        {{5, 0}, Interception::Hooks::D3D8::D3DFMT_A1R5G5B5, {5, 5}, 2, {5, 10}},
        {{5, 0}, Interception::Hooks::D3D8::D3DFMT_R5G6B5,   {6, 5}, 2, {5, 11}},
        {{8, 0}, Interception::Hooks::D3D8::D3DFMT_X8R8G8B8, {8, 8}, 4, {8, 16}},
        {{5, 0}, Interception::Hooks::D3D8::D3DFMT_X1R5G5B5, {5, 5}, 2, {5, 10}}
    };

    return result_vec;
}

const std::vector<uint32_t>& Replay::StereoReplayEngineDLL::PixelConverter::get_lookup_vec(const PixelLayout& in_layout)
{
    constexpr uint32_t GREEN_SHIFT       = 8;
    constexpr uint32_t MAX_CHANNEL_VALUE = 255;
    constexpr uint32_t N_VALUES          = 65536; /* of a 16-bit pixel */
    constexpr uint32_t OPAQUE_ALPHA_MASK = 0xff000000;
    constexpr uint32_t RED_SHIFT         = 16;

    /* The B, G, R and A of every 16-bit value, for each of the layouts, made when first needed. */
    static const std::vector<std::vector<uint32_t> > LOOKUP_VEC_VEC = [&]
    {
        std::vector<std::vector<uint32_t> > result_vec(get_layout_vec().size() );

        for (size_t n_layout = 0;
                    n_layout < result_vec.size();
                  ++n_layout)
        {
            const PixelLayout& layout = get_layout_vec().at(n_layout);

            if (layout.n_bytes == sizeof(uint16_t) )
            {
                const auto get_channel_func = [&](const uint32_t& in_value,
                                                  const Channel&  in_channel)
                {
                    const uint32_t max_value = (1u << in_channel.n_bits) - 1;

                    return ((in_value >> in_channel.shift) & max_value) * MAX_CHANNEL_VALUE / max_value;
                };

                result_vec.at(n_layout).resize(N_VALUES);

                for (uint32_t n_value = 0;
                              n_value < N_VALUES;
                            ++n_value)
                {
                    const uint32_t blue  = get_channel_func(n_value,
                                                            layout.blue);
                    const uint32_t green = get_channel_func(n_value,
                                                            layout.green);
                    const uint32_t red   = get_channel_func(n_value,
                                                            layout.red);

                    result_vec.at(n_layout).at(n_value) = blue | (green << GREEN_SHIFT) | (red << RED_SHIFT) | OPAQUE_ALPHA_MASK;
                }
            }
        }

        return result_vec;
    }();

    return LOOKUP_VEC_VEC.at(static_cast<size_t>(&in_layout - get_layout_vec().data() ) );
}
