/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Tests for GLTextureFormatMapper: what OpenGL calls the texels of a texture is a named format of the project, and the bytes of an upload have the size that its
 * format and type, and the pixel store, say.
 */
#include <gtest/gtest.h>
#include "analysis/api_dump_analysis/frontends/OpenGL/gl_texture_format_mapper.h"

namespace
{
    typedef Analysis::APIDumpAnalysis::Frontends::GLTextureFormatMapper Mapper;
    typedef Utils::Graphics::TextureFormat                              Format;
}

TEST(GLTextureFormatMapper, AnInternalFormatIsTheNamedFormatOfTheLayoutThatItMakes)
{
    EXPECT_EQ(Mapper::get_texture_format(0x8058),
              Format::R8G8B8A8_UNORM);              /* GL_RGBA8 */
    EXPECT_EQ(Mapper::get_texture_format(0x1908),
              Format::R8G8B8A8_UNORM);              /* GL_RGBA, which the drivers make of 8 bits for a channel */
    EXPECT_EQ(Mapper::get_texture_format(4),
              Format::R8G8B8A8_UNORM);              /* the number of components */
    EXPECT_EQ(Mapper::get_texture_format(3),
              Format::R8G8B8_UNORM);
    EXPECT_EQ(Mapper::get_texture_format(0x8050),
              Format::R5G6B5_UNORM);                /* GL_RGB5 */
    EXPECT_EQ(Mapper::get_texture_format(0x1906),
              Format::A8_UNORM);                    /* GL_ALPHA */
    EXPECT_EQ(Mapper::get_texture_format(0x8040),
              Format::L8_UNORM);                    /* GL_LUMINANCE8 */
    EXPECT_EQ(Mapper::get_texture_format(0x88F0),
              Format::D24_UNORM_S8_UINT);           /* GL_DEPTH24_STENCIL8 */
    EXPECT_EQ(Mapper::get_texture_format(0x8C43),
              Format::R8G8B8A8_UNORM_SRGB);         /* GL_SRGB8_ALPHA8 */
}

TEST(GLTextureFormatMapper, TheCompressedInternalFormatsAreTheBlockCompressedOnes)
{
    EXPECT_EQ(Mapper::get_texture_format(0x83F0),
              Format::BC1_UNORM);
    EXPECT_EQ(Mapper::get_texture_format(0x83F1),
              Format::BC1_UNORM);
    EXPECT_EQ(Mapper::get_texture_format(0x83F2),
              Format::BC2_UNORM);
    EXPECT_EQ(Mapper::get_texture_format(0x83F3),
              Format::BC3_UNORM);
}

TEST(GLTextureFormatMapper, AnInternalFormatThatIsNotFollowedIsUnknown)
{
    EXPECT_EQ(Mapper::get_texture_format(0xDEADBEEF),
              Format::UNKNOWN);
    EXPECT_EQ(Mapper::get_texture_format(0),
              Format::UNKNOWN);

    /* GL_SRGB does not say how many bits, and there is no named format of three bytes that is sRGB. */
    EXPECT_EQ(Mapper::get_texture_format(0x8C40),
              Format::UNKNOWN);
}

TEST(GLTextureFormatMapper, ATexelOfAnUploadHasTheBytesThatTheFormatAndTheTypeSay)
{
    EXPECT_EQ(Mapper::get_n_bytes_per_texel(0x1908, 0x1401),
              4u);   /* RGBA, bytes */
    EXPECT_EQ(Mapper::get_n_bytes_per_texel(0x1907, 0x1401),
              3u);   /* RGB, bytes */
    EXPECT_EQ(Mapper::get_n_bytes_per_texel(0x80E1, 0x1401),
              4u);   /* BGRA, bytes */
    EXPECT_EQ(Mapper::get_n_bytes_per_texel(0x1909, 0x1401),
              1u);   /* luminance */
    EXPECT_EQ(Mapper::get_n_bytes_per_texel(0x190A, 0x1401),
              2u);   /* luminance and alpha */
    EXPECT_EQ(Mapper::get_n_bytes_per_texel(0x1908, 0x1406),
              16u);  /* RGBA, floats */
    EXPECT_EQ(Mapper::get_n_bytes_per_texel(0x1908, 0x1403),
              8u);   /* RGBA, shorts */
}

TEST(GLTextureFormatMapper, ATypeThatPacksTheChannelsGivesTheWordAsTheWholeTexel)
{
    EXPECT_EQ(Mapper::get_n_bytes_per_texel(0x1907, 0x8363),
              2u);   /* RGB, 5 6 5 */
    EXPECT_EQ(Mapper::get_n_bytes_per_texel(0x1908, 0x8033),
              2u);   /* RGBA, 4 4 4 4 */
    EXPECT_EQ(Mapper::get_n_bytes_per_texel(0x1908, 0x8035),
              4u);   /* RGBA, 8 8 8 8 in a word */
    EXPECT_EQ(Mapper::get_n_bytes_per_texel(0x1907, 0x8032),
              1u);   /* RGB, 3 3 2 */
}

TEST(GLTextureFormatMapper, APairThatIsNotFollowedHasNoSize)
{
    EXPECT_EQ(Mapper::get_n_bytes_per_texel(0xDEAD, 0x1401),
              0u);
    EXPECT_EQ(Mapper::get_n_bytes_per_texel(0x1908, 0xBEEF),
              0u);
}

TEST(GLTextureFormatMapper, ARowStartsAtAMultipleOfTheAlignmentAndIsAsLongAsTheRowLengthSays)
{
    /* Three texels of three bytes: nine bytes, up to the alignment. */
    EXPECT_EQ(Mapper::get_unpack_row_pitch(3, 3, 0, 1),
              9u);
    EXPECT_EQ(Mapper::get_unpack_row_pitch(3, 3, 0, 2),
              10u);
    EXPECT_EQ(Mapper::get_unpack_row_pitch(3, 3, 0, 4),
              12u);
    EXPECT_EQ(Mapper::get_unpack_row_pitch(3, 3, 0, 8),
              16u);

    /* A row that is as long as it is is not changed. */
    EXPECT_EQ(Mapper::get_unpack_row_pitch(4, 4, 0, 4),
              16u);

    /* The length of the rows, when there is one, is the one that counts. */
    EXPECT_EQ(Mapper::get_unpack_row_pitch(3, 4, 10, 4),
              40u);

    /* No texel of a size that is known, no pitch. */
    EXPECT_EQ(Mapper::get_unpack_row_pitch(3, 0, 0, 4),
              0u);
}
