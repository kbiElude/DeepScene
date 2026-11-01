/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Tests for PixelConverter: the formats of a Direct3D 8 back buffer become 8-bit B, G, R and A.
 */
#include <gtest/gtest.h>
#include "Interception/Hooks/D3D8/types.h"
#include "pixel_converter.h"
#include <string.h>

TEST(PixelConverter, A32BitFormatKeepsItsColorsAndIsOpaque)
{
    /* Two pixels of 2x1, B, G, R, A in memory; the alpha of an X8R8G8B8 pixel is not part of the image. */
    const uint8_t        pixel_array[] = {10, 20, 30, 0,   40, 50, 60, 7};
    std::vector<uint8_t> out_vec;

    ASSERT_TRUE(Replay::StereoReplayEngineDLL::PixelConverter::convert_to_bgra8(Interception::Hooks::D3D8::D3DFMT_X8R8G8B8,
                                                                                2,
                                                                                1,
                                                                                sizeof(pixel_array),
                                                                                pixel_array,
                                                                               &out_vec) );
    EXPECT_EQ(out_vec,
              (std::vector<uint8_t>{10, 20, 30, 255,   40, 50, 60, 255}) );
}

TEST(PixelConverter, A16BitFormatIsExpandedToEightBitsPerChannel)
{
    /* R5G6B5: red 31, green 63, blue 31 is white; red 0, green 0, blue 31 is blue. */
    const uint16_t       pixel_array[] = {0xffff, 0x001f};
    std::vector<uint8_t> out_vec;

    ASSERT_TRUE(Replay::StereoReplayEngineDLL::PixelConverter::convert_to_bgra8(Interception::Hooks::D3D8::D3DFMT_R5G6B5,
                                                                                2,
                                                                                1,
                                                                                sizeof(pixel_array),
                                                                                reinterpret_cast<const uint8_t*>(pixel_array),
                                                                               &out_vec) );
    EXPECT_EQ(out_vec,
              (std::vector<uint8_t>{255, 255, 255, 255,   255, 0, 0, 255}) );
}

TEST(PixelConverter, TheFourBitAndFiveBitFormatsAreExpandedToo)
{
    const uint16_t       argb4444[] = {0xf0f0};         /* A 15, R 0, G 15, B 0 */
    const uint16_t       xrgb1555[] = {0x7c00};         /* R 31 */
    std::vector<uint8_t> out_vec;

    ASSERT_TRUE(Replay::StereoReplayEngineDLL::PixelConverter::convert_to_bgra8(Interception::Hooks::D3D8::D3DFMT_A4R4G4B4,
                                                                                1,
                                                                                1,
                                                                                sizeof(argb4444),
                                                                                reinterpret_cast<const uint8_t*>(argb4444),
                                                                               &out_vec) );
    EXPECT_EQ(out_vec,
              (std::vector<uint8_t>{0, 255, 0, 255}) );

    ASSERT_TRUE(Replay::StereoReplayEngineDLL::PixelConverter::convert_to_bgra8(Interception::Hooks::D3D8::D3DFMT_X1R5G5B5,
                                                                                1,
                                                                                1,
                                                                                sizeof(xrgb1555),
                                                                                reinterpret_cast<const uint8_t*>(xrgb1555),
                                                                               &out_vec) );
    EXPECT_EQ(out_vec,
              (std::vector<uint8_t>{0, 0, 255, 255}) );
}

TEST(PixelConverter, RowsAreReadWhereThePitchSaysAndWrittenWithoutPadding)
{
    /* 1x2 pixels, 8 bytes between the starts of the rows. */
    uint8_t              pixel_array[16] = {};
    std::vector<uint8_t> out_vec;

    pixel_array[0] = 1;
    pixel_array[1] = 2;
    pixel_array[2] = 3;
    pixel_array[8] = 4;
    pixel_array[9] = 5;
    pixel_array[10] = 6;

    ASSERT_TRUE(Replay::StereoReplayEngineDLL::PixelConverter::convert_to_bgra8(Interception::Hooks::D3D8::D3DFMT_A8R8G8B8,
                                                                                1,
                                                                                2,
                                                                                8,
                                                                                pixel_array,
                                                                               &out_vec) );
    EXPECT_EQ(out_vec,
              (std::vector<uint8_t>{1, 2, 3, 255,   4, 5, 6, 255}) );
}

TEST(PixelConverter, AFormatThatIsNotConvertedGivesNothing)
{
    const uint8_t        pixel_array[4] = {};
    std::vector<uint8_t> out_vec       = {1, 2, 3};

    EXPECT_FALSE(Replay::StereoReplayEngineDLL::PixelConverter::convert_to_bgra8(Interception::Hooks::D3D8::D3DFMT_UNKNOWN,
                                                                                 1,
                                                                                 1,
                                                                                 4,
                                                                                 pixel_array,
                                                                                &out_vec) );
    EXPECT_TRUE (out_vec.empty() );
}

/* The meaning of a pixel, from the definition of the format: every channel is scaled to 0..255. */
static uint8_t scale_channel(const uint32_t& in_value,
                             const uint32_t& in_shift,
                             const uint32_t& in_n_bits)
{
    const uint32_t max_value = (1u << in_n_bits) - 1;

    return static_cast<uint8_t>(((in_value >> in_shift) & max_value) * 255 / max_value);
}

TEST(PixelConverter, EveryValueOfEvery16BitFormatIsConvertedAsTheFormatSays)
{
    struct Format16
    {
        uint32_t blue_n_bits;
        uint32_t format;
        uint32_t green_n_bits;
        uint32_t green_shift;
        uint32_t red_n_bits;
        uint32_t red_shift;
    };

    constexpr uint32_t N_VALUES = 65536;

    const Format16 format_array[] =
    {
        {4, Interception::Hooks::D3D8::D3DFMT_A4R4G4B4, 4, 4, 4,  8},
        {5, Interception::Hooks::D3D8::D3DFMT_A1R5G5B5, 5, 5, 5, 10},
        {5, Interception::Hooks::D3D8::D3DFMT_R5G6B5,   6, 5, 5, 11},
        {5, Interception::Hooks::D3D8::D3DFMT_X1R5G5B5, 5, 5, 5, 10}
    };

    std::vector<uint16_t> value_vec(N_VALUES);

    for (uint32_t n_value = 0;
                  n_value < N_VALUES;
                ++n_value)
    {
        value_vec.at(n_value) = static_cast<uint16_t>(n_value);
    }

    for (const auto& format : format_array)
    {
        std::vector<uint8_t> out_vec;

        ASSERT_TRUE(Replay::StereoReplayEngineDLL::PixelConverter::convert_to_bgra8(format.format,
                                                                                    N_VALUES,
                                                                                    1,
                                                                                    N_VALUES * sizeof(uint16_t),
                                                                                    reinterpret_cast<const uint8_t*>(value_vec.data() ),
                                                                                   &out_vec) );
        ASSERT_EQ(out_vec.size(),
                  static_cast<size_t>(N_VALUES) * 4);

        for (uint32_t n_value = 0;
                      n_value < N_VALUES;
                    ++n_value)
        {
            const uint8_t* const pixel_ptr = out_vec.data() + static_cast<size_t>(n_value) * 4;

            ASSERT_EQ(pixel_ptr[0],
                      scale_channel(n_value, 0,                 format.blue_n_bits) ) << format.format << " " << n_value;
            ASSERT_EQ(pixel_ptr[1],
                      scale_channel(n_value, format.green_shift, format.green_n_bits) ) << format.format << " " << n_value;
            ASSERT_EQ(pixel_ptr[2],
                      scale_channel(n_value, format.red_shift,   format.red_n_bits) ) << format.format << " " << n_value;
            ASSERT_EQ(pixel_ptr[3],
                      255) << format.format << " " << n_value;
        }
    }
}

TEST(PixelConverter, RowsWithPaddingAreReadByTheirPitchAndTheOutputHasNone)
{
    constexpr uint32_t HEIGHT = 3;
    constexpr uint32_t PITCH  = 20; /* two pixels of 4 bytes, and 12 bytes of padding */
    constexpr uint32_t WIDTH  = 2;

    std::vector<uint8_t> in_vec(PITCH * HEIGHT,
                                0xee);
    std::vector<uint8_t> out_vec;

    for (uint32_t n_row = 0;
                  n_row < HEIGHT;
                ++n_row)
    {
        for (uint32_t n_column = 0;
                      n_column < WIDTH;
                    ++n_column)
        {
            uint8_t* const pixel_ptr = in_vec.data() + n_row * PITCH + n_column * 4;

            pixel_ptr[0] = static_cast<uint8_t>(n_row * 10 + n_column);
            pixel_ptr[1] = 2;
            pixel_ptr[2] = 3;
            pixel_ptr[3] = 0;
        }
    }

    ASSERT_TRUE(Replay::StereoReplayEngineDLL::PixelConverter::convert_to_bgra8(Interception::Hooks::D3D8::D3DFMT_X8R8G8B8,
                                                                                WIDTH,
                                                                                HEIGHT,
                                                                                PITCH,
                                                                                in_vec.data(),
                                                                               &out_vec) );
    ASSERT_EQ(out_vec.size(),
              static_cast<size_t>(WIDTH * HEIGHT * 4) );

    for (uint32_t n_row = 0;
                  n_row < HEIGHT;
                ++n_row)
    {
        for (uint32_t n_column = 0;
                      n_column < WIDTH;
                    ++n_column)
        {
            const uint8_t* const pixel_ptr = out_vec.data() + (n_row * WIDTH + n_column) * 4;

            EXPECT_EQ(pixel_ptr[0],
                      n_row * 10 + n_column);
            EXPECT_EQ(pixel_ptr[1],
                      2);
            EXPECT_EQ(pixel_ptr[2],
                      3);
            EXPECT_EQ(pixel_ptr[3],
                      255);
        }
    }
}

TEST(PixelConverter, AFormatWhichIsNotConvertedLeavesTheOutputEmptyEvenIfItHeldAnImage)
{
    const uint8_t        pixel_array[4] = {};
    std::vector<uint8_t> out_vec(100,
                                 1);

    EXPECT_FALSE(Replay::StereoReplayEngineDLL::PixelConverter::convert_to_bgra8(Interception::Hooks::D3D8::D3DFMT_D24S8,
                                                                                 1,
                                                                                 1,
                                                                                 sizeof(pixel_array),
                                                                                 pixel_array,
                                                                                &out_vec) );
    EXPECT_TRUE(out_vec.empty() );
}
