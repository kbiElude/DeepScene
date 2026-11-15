/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * A texture format that belongs to no graphics API. Each format names one memory layout that the programs of this project handle and describes it: its
 * block size, its bytes per block, and whether it is compressed, has depth or stencil, or is sRGB. The front end of each API converts its own values
 * (an OpenGL internal format and type, a Direct3D format or pixel format) to one of these. A value without a counterpart becomes UNKNOWN, and the program
 * keeps the API's original value next to it to record what it was.
 *
 * The values are stored in files and never change: a new format is added after the last one, and none is removed or moved. The names state the memory
 * layout. For byte formats the channels follow the byte order, lowest address first (R8G8B8A8 is one byte each of red, green, blue and alpha). For formats
 * packed into a word the channels follow the bit order of the word, highest bits first (R5G6B5 has red in the top five bits). X is an unused part, and L
 * is luminance (one value for red, green and blue).
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

            /* 16 and 32 bits per channel */
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

        /* The value of the last format. The values run from 0 to this one with no gaps. */
        constexpr uint16_t LAST_TEXTURE_FORMAT_VALUE = 41;

        /* The properties of a format. For an uncompressed format a block is one texel by one texel. D32_FLOAT_S8_UINT has the D3D and Vulkan memory
         * layout: 8 bytes per texel (the float, the stencil byte, and padding).
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
            uint32_t    n_bytes_per_block      = 0; /* 0 for UNKNOWN */
        };

        class TextureFormats
        {
        public:
            /* Returns the properties of the format. UNKNOWN has no bytes per block. */
            static TextureFormatInfo get_info(const TextureFormat& in_format);

            /* Returns the bytes that a region of texels occupies. It counts whole blocks, so a region smaller than one block still takes one block.
             * Returns 0 for an unknown format.
             */
            static uint64_t get_n_bytes(const TextureFormat& in_format,
                                        const uint32_t&      in_width_in_texels,
                                        const uint32_t&      in_height_in_texels);

            /* True if the value is a valid format, UNKNOWN included. Used to validate values read from a file. */
            static bool is_format_value(const uint32_t& in_value);

        private:
            /* Builds the properties for one table row from: block width, block height, depth, stencil, compressed, sRGB, name, bytes per block. */
            static TextureFormatInfo make_info(const uint32_t& in_block_width,
                                               const uint32_t& in_block_height,
                                               const bool&     in_has_depth,
                                               const bool&     in_has_stencil,
                                               const bool&     in_is_compressed,
                                               const bool&     in_is_srgb,
                                               const char*     in_name_ptr,
                                               const uint32_t& in_n_bytes);
        };
    }
}

#endif /* UTILS_GRAPHICS_TEXTURE_FORMAT_H */
