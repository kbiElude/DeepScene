/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_analysis/frontends/OpenGL/gl_texture_format_mapper.h"

namespace
{
    typedef Utils::Graphics::TextureFormat Format;

    struct ComponentRow
    {
        uint32_t format;
        uint32_t n_components;
    };

    struct InternalFormatRow
    {
        uint32_t internal_format;
        Format   texture_format;
    };

    struct TypeRow
    {
        uint32_t n_bytes;
        bool     is_packed; /* the channels are in a word of the size: that is all of the texel */
        uint32_t type;
    };

    const ComponentRow COMPONENT_ROW_ARRAY[] =
    {
        {0x1901, 1}, /* GL_STENCIL_INDEX    */
        {0x1902, 1}, /* GL_DEPTH_COMPONENT  */
        {0x1903, 1}, /* GL_RED              */
        {0x1906, 1}, /* GL_ALPHA            */
        {0x1907, 3}, /* GL_RGB              */
        {0x1908, 4}, /* GL_RGBA             */
        {0x1909, 1}, /* GL_LUMINANCE        */
        {0x190A, 2}, /* GL_LUMINANCE_ALPHA  */
        {0x80E0, 3}, /* GL_BGR              */
        {0x80E1, 4}  /* GL_BGRA             */
    };

    /* The unsized ones are what the drivers make of them. */
    const InternalFormatRow INTERNAL_FORMAT_ROW_ARRAY[] =
    {
        {0x1902, Format::D24_UNORM_X8},          /* GL_DEPTH_COMPONENT              */
        {0x1906, Format::A8_UNORM},              /* GL_ALPHA                        */
        {0x1907, Format::R8G8B8_UNORM},          /* GL_RGB                          */
        {0x1908, Format::R8G8B8A8_UNORM},        /* GL_RGBA                         */
        {0x1909, Format::L8_UNORM},              /* GL_LUMINANCE                    */
        {0x190A, Format::L8A8_UNORM},            /* GL_LUMINANCE_ALPHA              */
        {1,      Format::L8_UNORM},              /* the number of components: 1     */
        {2,      Format::L8A8_UNORM},            /* 2                               */
        {3,      Format::R8G8B8_UNORM},          /* 3                               */
        {4,      Format::R8G8B8A8_UNORM},        /* 4                               */
        {0x803C, Format::A8_UNORM},              /* GL_ALPHA8                       */
        {0x8040, Format::L8_UNORM},              /* GL_LUMINANCE8                   */
        {0x8045, Format::L8A8_UNORM},            /* GL_LUMINANCE8_ALPHA8            */
        {0x8050, Format::R5G6B5_UNORM},          /* GL_RGB5                         */
        {0x8051, Format::R8G8B8_UNORM},          /* GL_RGB8                         */
        {0x8056, Format::A4R4G4B4_UNORM},        /* GL_RGBA4                        */
        {0x8057, Format::A1R5G5B5_UNORM},        /* GL_RGB5_A1                      */
        {0x8058, Format::R8G8B8A8_UNORM},        /* GL_RGBA8                        */
        {0x8059, Format::A2B10G10R10_UNORM},     /* GL_RGB10_A2                     */
        {0x805B, Format::R16G16B16A16_UNORM},    /* GL_RGBA16                       */
        {0x81A5, Format::D16_UNORM},             /* GL_DEPTH_COMPONENT16            */
        {0x81A6, Format::D24_UNORM_X8},          /* GL_DEPTH_COMPONENT24            */
        {0x822D, Format::R16_FLOAT},             /* GL_R16F                         */
        {0x822E, Format::R32_FLOAT},             /* GL_R32F                         */
        {0x83F0, Format::BC1_UNORM},             /* GL_COMPRESSED_RGB_S3TC_DXT1     */
        {0x83F1, Format::BC1_UNORM},             /* GL_COMPRESSED_RGBA_S3TC_DXT1    */
        {0x83F2, Format::BC2_UNORM},             /* GL_COMPRESSED_RGBA_S3TC_DXT3    */
        {0x83F3, Format::BC3_UNORM},             /* GL_COMPRESSED_RGBA_S3TC_DXT5    */
        {0x8814, Format::R32G32B32A32_FLOAT},    /* GL_RGBA32F                      */
        {0x881A, Format::R16G16B16A16_FLOAT},    /* GL_RGBA16F                      */
        {0x88F0, Format::D24_UNORM_S8_UINT},     /* GL_DEPTH24_STENCIL8             */
        {0x8C43, Format::R8G8B8A8_UNORM_SRGB},   /* GL_SRGB8_ALPHA8                 */
        {0x8D62, Format::R5G6B5_UNORM}           /* GL_RGB565                       */
    };

    const TypeRow TYPE_ROW_ARRAY[] =
    {
        {1, false, 0x1400}, /* GL_BYTE                       */
        {1, false, 0x1401}, /* GL_UNSIGNED_BYTE              */
        {2, false, 0x1402}, /* GL_SHORT                      */
        {2, false, 0x1403}, /* GL_UNSIGNED_SHORT             */
        {4, false, 0x1404}, /* GL_INT                        */
        {4, false, 0x1405}, /* GL_UNSIGNED_INT               */
        {4, false, 0x1406}, /* GL_FLOAT                      */
        {1, true,  0x8032}, /* GL_UNSIGNED_BYTE_3_3_2        */
        {2, true,  0x8033}, /* GL_UNSIGNED_SHORT_4_4_4_4     */
        {2, true,  0x8034}, /* GL_UNSIGNED_SHORT_5_5_5_1     */
        {4, true,  0x8035}, /* GL_UNSIGNED_INT_8_8_8_8       */
        {4, true,  0x8036}, /* GL_UNSIGNED_INT_10_10_10_2    */
        {2, true,  0x8363}, /* GL_UNSIGNED_SHORT_5_6_5       */
        {2, true,  0x8364}, /* GL_UNSIGNED_SHORT_5_6_5_REV   */
        {2, true,  0x8365}, /* GL_UNSIGNED_SHORT_4_4_4_4_REV */
        {2, true,  0x8366}, /* GL_UNSIGNED_SHORT_1_5_5_5_REV */
        {4, true,  0x8367}, /* GL_UNSIGNED_INT_8_8_8_8_REV   */
        {4, true,  0x8368}  /* GL_UNSIGNED_INT_2_10_10_10_REV */
    };
}

uint32_t Analysis::APIDumpAnalysis::Frontends::GLTextureFormatMapper::get_n_bytes_per_texel(const uint32_t& in_format,
                                                                                            const uint32_t& in_type)
{
    uint32_t n_components = 0;
    uint32_t result       = 0;

    for (const auto& row : COMPONENT_ROW_ARRAY)
    {
        if (row.format == in_format)
        {
            n_components = row.n_components;
        }
    }

    for (const auto& row : TYPE_ROW_ARRAY)
    {
        if (row.type == in_type &&
            n_components != 0)
        {
            result = row.is_packed ? row.n_bytes
                                   : row.n_bytes * n_components;
        }
    }

    return result;
}

Utils::Graphics::TextureFormat Analysis::APIDumpAnalysis::Frontends::GLTextureFormatMapper::get_texture_format(const uint32_t& in_internal_format)
{
    Format result = Format::UNKNOWN;

    for (const auto& row : INTERNAL_FORMAT_ROW_ARRAY)
    {
        if (row.internal_format == in_internal_format)
        {
            result = row.texture_format;
        }
    }

    return result;
}

uint32_t Analysis::APIDumpAnalysis::Frontends::GLTextureFormatMapper::get_unpack_row_pitch(const uint32_t& in_width,
                                                                                           const uint32_t& in_n_bytes_per_texel,
                                                                                           const uint32_t& in_row_length,
                                                                                           const uint32_t& in_alignment)
{
    const uint32_t n_texels = (in_row_length > 0) ? in_row_length
                                                  : in_width;
    const uint32_t row_n_bytes = n_texels * in_n_bytes_per_texel;
    uint32_t       result      = 0;

    if (in_n_bytes_per_texel != 0)
    {
        /* A row starts at a multiple of the alignment: the alignment is a power of two. */
        result = (in_alignment > 1) ? ((row_n_bytes + in_alignment - 1) / in_alignment) * in_alignment
                                    : row_n_bytes;
    }

    return result;
}
