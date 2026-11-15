/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "utils/graphics/texture_format.h"

Utils::Graphics::TextureFormatInfo Utils::Graphics::TextureFormats::get_info(const TextureFormat& in_format)
{
    typedef Utils::Graphics::TextureFormat     Format;
    typedef Utils::Graphics::TextureFormatInfo Info;

    /* A format together with its properties. */
    struct Row
    {
        Format format;
        Info   info;
    };

    /* The rows follow the order of the enum values, so a format's value is its row index. The tests verify this. */
    static const Row ROW_ARRAY[] =
    {
        {Format::UNKNOWN,             make_info(1, 1, false, false, false, false, "UNKNOWN",             0)},
        {Format::R8_UNORM,            make_info(1, 1, false, false, false, false, "R8_UNORM",            1)},
        {Format::A8_UNORM,            make_info(1, 1, false, false, false, false, "A8_UNORM",            1)},
        {Format::L8_UNORM,            make_info(1, 1, false, false, false, false, "L8_UNORM",            1)},
        {Format::R8G8_UNORM,          make_info(1, 1, false, false, false, false, "R8G8_UNORM",          2)},
        {Format::L8A8_UNORM,          make_info(1, 1, false, false, false, false, "L8A8_UNORM",          2)},
        {Format::R5G6B5_UNORM,        make_info(1, 1, false, false, false, false, "R5G6B5_UNORM",        2)},
        {Format::A1R5G5B5_UNORM,      make_info(1, 1, false, false, false, false, "A1R5G5B5_UNORM",      2)},
        {Format::X1R5G5B5_UNORM,      make_info(1, 1, false, false, false, false, "X1R5G5B5_UNORM",      2)},
        {Format::A4R4G4B4_UNORM,      make_info(1, 1, false, false, false, false, "A4R4G4B4_UNORM",      2)},
        {Format::R8G8B8_UNORM,        make_info(1, 1, false, false, false, false, "R8G8B8_UNORM",        3)},
        {Format::B8G8R8_UNORM,        make_info(1, 1, false, false, false, false, "B8G8R8_UNORM",        3)},
        {Format::R8G8B8A8_UNORM,      make_info(1, 1, false, false, false, false, "R8G8B8A8_UNORM",      4)},
        {Format::B8G8R8A8_UNORM,      make_info(1, 1, false, false, false, false, "B8G8R8A8_UNORM",      4)},
        {Format::B8G8R8X8_UNORM,      make_info(1, 1, false, false, false, false, "B8G8R8X8_UNORM",      4)},
        {Format::R8G8B8A8_UNORM_SRGB, make_info(1, 1, false, false, false, true,  "R8G8B8A8_UNORM_SRGB", 4)},
        {Format::B8G8R8A8_UNORM_SRGB, make_info(1, 1, false, false, false, true,  "B8G8R8A8_UNORM_SRGB", 4)},
        {Format::A2B10G10R10_UNORM,   make_info(1, 1, false, false, false, false, "A2B10G10R10_UNORM",   4)},
        {Format::R16_UNORM,           make_info(1, 1, false, false, false, false, "R16_UNORM",           2)},
        {Format::R16G16_UNORM,        make_info(1, 1, false, false, false, false, "R16G16_UNORM",        4)},
        {Format::R16G16B16A16_UNORM,  make_info(1, 1, false, false, false, false, "R16G16B16A16_UNORM",  8)},
        {Format::R16_FLOAT,           make_info(1, 1, false, false, false, false, "R16_FLOAT",           2)},
        {Format::R16G16B16A16_FLOAT,  make_info(1, 1, false, false, false, false, "R16G16B16A16_FLOAT",  8)},
        {Format::R32_FLOAT,           make_info(1, 1, false, false, false, false, "R32_FLOAT",           4)},
        {Format::R32G32B32A32_FLOAT,  make_info(1, 1, false, false, false, false, "R32G32B32A32_FLOAT", 16)},
        {Format::D16_UNORM,           make_info(1, 1, true,  false, false, false, "D16_UNORM",           2)},
        {Format::D24_UNORM_S8_UINT,   make_info(1, 1, true,  true,  false, false, "D24_UNORM_S8_UINT",   4)},
        {Format::D24_UNORM_X8,        make_info(1, 1, true,  false, false, false, "D24_UNORM_X8",        4)},
        {Format::D32_FLOAT,           make_info(1, 1, true,  false, false, false, "D32_FLOAT",           4)},
        {Format::D32_FLOAT_S8_UINT,   make_info(1, 1, true,  true,  false, false, "D32_FLOAT_S8_UINT",   8)},
        {Format::S8_UINT,             make_info(1, 1, false, true,  false, false, "S8_UINT",             1)},
        {Format::BC1_UNORM,           make_info(4, 4, false, false, true,  false, "BC1_UNORM",           8)},
        {Format::BC1_UNORM_SRGB,      make_info(4, 4, false, false, true,  true,  "BC1_UNORM_SRGB",      8)},
        {Format::BC2_UNORM,           make_info(4, 4, false, false, true,  false, "BC2_UNORM",          16)},
        {Format::BC2_UNORM_SRGB,      make_info(4, 4, false, false, true,  true,  "BC2_UNORM_SRGB",     16)},
        {Format::BC3_UNORM,           make_info(4, 4, false, false, true,  false, "BC3_UNORM",          16)},
        {Format::BC3_UNORM_SRGB,      make_info(4, 4, false, false, true,  true,  "BC3_UNORM_SRGB",     16)},
        {Format::BC4_UNORM,           make_info(4, 4, false, false, true,  false, "BC4_UNORM",           8)},
        {Format::BC5_UNORM,           make_info(4, 4, false, false, true,  false, "BC5_UNORM",          16)},
        {Format::BC6H_UF16,           make_info(4, 4, false, false, true,  false, "BC6H_UF16",          16)},
        {Format::BC7_UNORM,           make_info(4, 4, false, false, true,  false, "BC7_UNORM",          16)},
        {Format::BC7_UNORM_SRGB,      make_info(4, 4, false, false, true,  true,  "BC7_UNORM_SRGB",     16)}
    };

    constexpr size_t N_ROWS = sizeof(ROW_ARRAY) / sizeof(ROW_ARRAY[0]);

    static_assert(N_ROWS == static_cast<size_t>(Utils::Graphics::LAST_TEXTURE_FORMAT_VALUE) + 1,
                  "The table has a row for each format");

    const size_t n_row = static_cast<size_t>(in_format);

    return (n_row < N_ROWS) ? ROW_ARRAY[n_row].info
                            : ROW_ARRAY[0].info;
}

uint64_t Utils::Graphics::TextureFormats::get_n_bytes(const TextureFormat& in_format,
                                                      const uint32_t&      in_width_in_texels,
                                                      const uint32_t&      in_height_in_texels)
{
    const TextureFormatInfo info = get_info(in_format);

    /* Counts whole blocks: a width of 5 texels with 4-texel blocks is 2 blocks. */
    const uint64_t n_blocks_across = (static_cast<uint64_t>(in_width_in_texels)  + info.block_width_in_texels  - 1) / info.block_width_in_texels;
    const uint64_t n_blocks_down   = (static_cast<uint64_t>(in_height_in_texels) + info.block_height_in_texels - 1) / info.block_height_in_texels;

    return n_blocks_across * n_blocks_down * info.n_bytes_per_block;
}

bool Utils::Graphics::TextureFormats::is_format_value(const uint32_t& in_value)
{
    return (in_value <= LAST_TEXTURE_FORMAT_VALUE);
}

Utils::Graphics::TextureFormatInfo Utils::Graphics::TextureFormats::make_info(const uint32_t& in_block_width,
                                                                              const uint32_t& in_block_height,
                                                                              const bool&     in_has_depth,
                                                                              const bool&     in_has_stencil,
                                                                              const bool&     in_is_compressed,
                                                                              const bool&     in_is_srgb,
                                                                              const char*     in_name_ptr,
                                                                              const uint32_t& in_n_bytes)
{
    return TextureFormatInfo{in_block_height, in_block_width, in_has_depth, in_has_stencil, in_is_compressed, in_is_srgb, in_name_ptr, in_n_bytes};
}
