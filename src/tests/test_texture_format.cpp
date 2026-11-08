/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Tests for TextureFormat: the table says what each format is, the sizes of regions are of whole blocks, and what is not a format is told.
 */
#include <gtest/gtest.h>
#include "utils/graphics/texture_format.h"
#include <set>
#include <string>

namespace
{
    typedef Utils::Graphics::TextureFormat  Format;
    typedef Utils::Graphics::TextureFormats Formats;
}

TEST(TextureFormats, EveryValueFromZeroToTheLastIsAFormatWithAnameOfItsOwnAndTheNextIsNot)
{
    std::set<std::string> name_set;

    for (uint32_t value = 0; value <= Utils::Graphics::LAST_TEXTURE_FORMAT_VALUE; ++value)
    {
        const auto info = Formats::get_info(static_cast<Format>(value) );

        EXPECT_TRUE(Formats::is_format_value(value) ) << value;
        ASSERT_NE(info.name_ptr,
                  nullptr);

        name_set.insert(info.name_ptr);

        EXPECT_GE(info.block_width_in_texels,  1u) << info.name_ptr;
        EXPECT_GE(info.block_height_in_texels, 1u) << info.name_ptr;

        /* What is known has bytes; the one that is not has none. */
        EXPECT_EQ(info.n_bytes_per_block != 0,
                  value != 0) << info.name_ptr;
    }

    EXPECT_EQ(name_set.size(),
              static_cast<size_t>(Utils::Graphics::LAST_TEXTURE_FORMAT_VALUE) + 1);
    EXPECT_FALSE(Formats::is_format_value(Utils::Graphics::LAST_TEXTURE_FORMAT_VALUE + 1) );
}

TEST(TextureFormats, ThePlaceOfAFormatInTheTableIsItsValueAndItsNameIsItsIdentifier)
{
    EXPECT_STREQ(Formats::get_info(Format::UNKNOWN).name_ptr,            "UNKNOWN");
    EXPECT_STREQ(Formats::get_info(Format::R8_UNORM).name_ptr,           "R8_UNORM");
    EXPECT_STREQ(Formats::get_info(Format::R5G6B5_UNORM).name_ptr,       "R5G6B5_UNORM");
    EXPECT_STREQ(Formats::get_info(Format::B8G8R8X8_UNORM).name_ptr,     "B8G8R8X8_UNORM");
    EXPECT_STREQ(Formats::get_info(Format::A2B10G10R10_UNORM).name_ptr,  "A2B10G10R10_UNORM");
    EXPECT_STREQ(Formats::get_info(Format::R32G32B32A32_FLOAT).name_ptr, "R32G32B32A32_FLOAT");
    EXPECT_STREQ(Formats::get_info(Format::D24_UNORM_S8_UINT).name_ptr,  "D24_UNORM_S8_UINT");
    EXPECT_STREQ(Formats::get_info(Format::BC1_UNORM).name_ptr,          "BC1_UNORM");
    EXPECT_STREQ(Formats::get_info(Format::BC7_UNORM_SRGB).name_ptr,     "BC7_UNORM_SRGB");
}

TEST(TextureFormats, AFormatSaysWhatItIs)
{
    EXPECT_TRUE (Formats::get_info(Format::BC3_UNORM).is_compressed);
    EXPECT_FALSE(Formats::get_info(Format::R8G8B8A8_UNORM).is_compressed);

    EXPECT_TRUE (Formats::get_info(Format::R8G8B8A8_UNORM_SRGB).is_srgb);
    EXPECT_TRUE (Formats::get_info(Format::BC1_UNORM_SRGB).is_srgb);
    EXPECT_FALSE(Formats::get_info(Format::B8G8R8A8_UNORM).is_srgb);

    EXPECT_TRUE (Formats::get_info(Format::D24_UNORM_S8_UINT).has_depth);
    EXPECT_TRUE (Formats::get_info(Format::D24_UNORM_S8_UINT).has_stencil);
    EXPECT_TRUE (Formats::get_info(Format::D32_FLOAT).has_depth);
    EXPECT_FALSE(Formats::get_info(Format::D32_FLOAT).has_stencil);
    EXPECT_TRUE (Formats::get_info(Format::S8_UINT).has_stencil);
    EXPECT_FALSE(Formats::get_info(Format::S8_UINT).has_depth);
    EXPECT_FALSE(Formats::get_info(Format::R8G8B8A8_UNORM).has_depth);

    /* The compressed ones are blocks of four by four texels, and nothing else is. */
    for (uint32_t value = 1; value <= Utils::Graphics::LAST_TEXTURE_FORMAT_VALUE; ++value)
    {
        const auto info = Formats::get_info(static_cast<Format>(value) );

        EXPECT_EQ(info.block_width_in_texels,
                  info.is_compressed ? 4u : 1u) << info.name_ptr;
        EXPECT_EQ(info.block_height_in_texels,
                  info.is_compressed ? 4u : 1u) << info.name_ptr;
    }
}

TEST(TextureFormats, ARegionTakesWholeBlocks)
{
    /* Texels of bytes. */
    EXPECT_EQ(Formats::get_n_bytes(Format::R8G8B8A8_UNORM, 3, 2),
              24u);
    EXPECT_EQ(Formats::get_n_bytes(Format::R5G6B5_UNORM, 10, 10),
              200u);
    EXPECT_EQ(Formats::get_n_bytes(Format::R32G32B32A32_FLOAT, 2, 2),
              64u);

    /* Blocks of four by four: a part of a block is a block, so the last levels of a compressed texture are of one block each. */
    EXPECT_EQ(Formats::get_n_bytes(Format::BC1_UNORM, 8, 8),
              32u);
    EXPECT_EQ(Formats::get_n_bytes(Format::BC1_UNORM, 5, 5),
              32u);
    EXPECT_EQ(Formats::get_n_bytes(Format::BC1_UNORM, 1, 1),
              8u);
    EXPECT_EQ(Formats::get_n_bytes(Format::BC3_UNORM, 4, 4),
              16u);
    EXPECT_EQ(Formats::get_n_bytes(Format::BC7_UNORM, 9, 3),
              48u);

    /* Nothing, and what is not known. */
    EXPECT_EQ(Formats::get_n_bytes(Format::BC1_UNORM, 0, 8),
              0u);
    EXPECT_EQ(Formats::get_n_bytes(Format::UNKNOWN, 64, 64),
              0u);
}

TEST(TextureFormats, AValueThatIsNotAFormatIsTakenForTheUnknownOne)
{
    const auto info = Formats::get_info(static_cast<Format>(Utils::Graphics::LAST_TEXTURE_FORMAT_VALUE + 7) );

    EXPECT_STREQ(info.name_ptr,
                 "UNKNOWN");
    EXPECT_EQ(Formats::get_n_bytes(static_cast<Format>(1000),
                                   16,
                                   16),
              0u);
}
