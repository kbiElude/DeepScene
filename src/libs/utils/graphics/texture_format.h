/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * What the texels of a texture are, in the words of no API: a named format for every layout that the programs of this project follow, with what each says of
 * itself (the size of its block, the bytes of it, whether it is compressed, has depth or stencil, is sRGB). The front end of each API turns its own values (an
 * OpenGL internal format and type, a Direct3D format or pixel format) into one of these, and what it cannot is UNKNOWN: the program that has it keeps the
 * value of the API beside it, to say what it was.
 *
 * The values are stored in files and do not change: a format is added after the last one, and none is removed or moved. The names tell the layout in memory.
 * For formats of bytes the channels are in the order of the bytes, the lowest address first (R8G8B8A8 is a byte of red, one of green, one of blue and one of
 * alpha); for formats packed in a word they are in the order of the bits of the word, the highest first (R5G6B5 has red in the top five bits). X is a part
 * that is not used, and L is luminance (one value for red, green and blue).
 */
#ifndef UTILS_GRAPHICS_TEXTURE_FORMAT_H
#define UTILS_GRAPHICS_TEXTURE_FORMAT_H

#include <stdint.h>

namespace Utils
{
    namespace Graphics
    {
        enum class TextureFormat : uint16_t
        {
            UNKNOWN = 0,

            /* Bytes */
            R8_UNORM           = 1,
            A8_UNORM           = 2,
            L8_UNORM           = 3,
            R8G8_UNORM         = 4,
            L8A8_UNORM         = 5,
            R8G8B8_UNORM       = 10,
            B8G8R8_UNORM       = 11,
            R8G8B8A8_UNORM     = 12,
            B8G8R8A8_UNORM     = 13,
            B8G8R8X8_UNORM     = 14,
            R8G8B8A8_UNORM_SRGB = 15,
            B8G8R8A8_UNORM_SRGB = 16,

            /* Packed in a word */
            R5G6B5_UNORM       = 6,
            A1R5G5B5_UNORM     = 7,
            X1R5G5B5_UNORM     = 8,
            A4R4G4B4_UNORM     = 9,
            A2B10G10R10_UNORM  = 17,

            /* Of 16 and 32 bits for a channel */
            R16_UNORM          = 18,
            R16G16_UNORM       = 19,
            R16G16B16A16_UNORM = 20,
            R16_FLOAT          = 21,
            R16G16B16A16_FLOAT = 22,
            R32_FLOAT          = 23,
            R32G32B32A32_FLOAT = 24,

            /* Depth and stencil */
            D16_UNORM          = 25,
            D24_UNORM_S8_UINT  = 26,
            D24_UNORM_X8       = 27,
            D32_FLOAT          = 28,
            D32_FLOAT_S8_UINT  = 29,
            S8_UINT            = 30,

            /* Compressed, in blocks of 4 by 4 texels */
            BC1_UNORM          = 31,
            BC1_UNORM_SRGB     = 32,
            BC2_UNORM          = 33,
            BC2_UNORM_SRGB     = 34,
            BC3_UNORM          = 35,
            BC3_UNORM_SRGB     = 36,
            BC4_UNORM          = 37,
            BC5_UNORM          = 38,
            BC6H_UF16          = 39,
            BC7_UNORM          = 40,
            BC7_UNORM_SRGB     = 41
        };

        /* The value of the last format: the formats go from 0 to it, with none missing. */
        constexpr uint16_t LAST_TEXTURE_FORMAT_VALUE = 41;

        /* What a format says of itself. A texel is a block of one by one for a format that is not compressed. D32_FLOAT_S8_UINT is as D3D and Vulkan have it in
         * memory: 8 bytes for a texel (the float, the stencil byte, and padding).
         */
        struct TextureFormatInfo
        {
            uint32_t    block_height_in_texels = 1;
            uint32_t    block_width_in_texels  = 1;
            bool        has_depth              = false;
            bool        has_stencil            = false;
            bool        is_compressed          = false;
            bool        is_srgb                = false;
            const char* name_ptr               = "UNKNOWN";
            uint32_t    n_bytes_per_block      = 0; /* 0: not known (UNKNOWN) */
        };

        class TextureFormats
        {
        public:
            /* What the format says of itself; the UNKNOWN has no bytes. */
            static TextureFormatInfo get_info(const TextureFormat& in_format);

            /* The bytes that a region of the texels takes: whole blocks, so that a region smaller than a block takes the one. 0 for a format that is not known. */
            static uint64_t get_n_bytes(const TextureFormat& in_format,
                                        const uint32_t&      in_width_in_texels,
                                        const uint32_t&      in_height_in_texels);

            /* Whether the value is that of a format (the UNKNOWN is). For what is read from a file. */
            static bool is_format_value(const uint32_t& in_value);
        };
    }
}

#endif /* UTILS_GRAPHICS_TEXTURE_FORMAT_H */
