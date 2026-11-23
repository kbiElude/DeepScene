/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Tests for the bars that PixelRowWriter makes black around the picture of an application, and for the alpha it makes opaque, in the image of two eyes side by side.
 */
#include <gtest/gtest.h>
#include "pixel_row_writer.h"
#include <vector>

namespace
{
    constexpr uint32_t BLACK_BGRA = 0xFF000000;
    constexpr uint32_t WHITE_BGRA = 0xFFFFFFFF;

    /* Two eyes of width x height side by side, all white. */
    std::vector<uint32_t> make_white_image(const uint32_t& in_width,
                                           const uint32_t& in_height)
    {
        return std::vector<uint32_t>(static_cast<size_t>(in_width) * 2 * in_height,
                                     WHITE_BGRA);
    }
}

TEST(PixelRowWriterTest, TheBarsAreBlackAndThePictureIsKept)
{
    constexpr uint32_t WIDTH  = 8;
    constexpr uint32_t HEIGHT = 6;

    std::vector<uint32_t>   image = make_white_image(WIDTH,
                                                     HEIGHT);
    Stereo::Core::PixelRect picture;

    picture.height = 4;
    picture.width  = 4;
    picture.x      = 2;
    picture.y      = 1;

    Stereo::ConverterDLL::PixelRowWriter::blank_bars(picture,
                                                     WIDTH,
                                                     HEIGHT,
                                                     1, /* in_eye */
                                                     reinterpret_cast<uint8_t*>(image.data() ),
                                                     WIDTH * 2 * sizeof(uint32_t) );

    for (uint32_t n_row = 0;
                  n_row < HEIGHT;
                ++n_row)
    {
        for (uint32_t n_column = 0;
                      n_column < WIDTH;
                    ++n_column)
        {
            const bool     is_inside = (n_row    >= 1 && n_row    < 5 &&
                                        n_column >= 2 && n_column < 6);
            const uint32_t left_eye  = image.at(static_cast<size_t>(n_row) * WIDTH * 2 + n_column);
            const uint32_t right_eye = image.at(static_cast<size_t>(n_row) * WIDTH * 2 + WIDTH + n_column);

            EXPECT_EQ(left_eye,
                      WHITE_BGRA) << "the other eye is not touched: " << n_row << ", " << n_column;
            EXPECT_EQ(right_eye,
                      (is_inside) ? WHITE_BGRA
                                  : BLACK_BGRA) << n_row << ", " << n_column;
        }
    }
}

TEST(PixelRowWriterTest, NothingChangesWhenThePictureIsTheWholeImageOrIsEmpty)
{
    constexpr uint32_t WIDTH  = 4;
    constexpr uint32_t HEIGHT = 3;

    std::vector<uint32_t>   image = make_white_image(WIDTH,
                                                     HEIGHT);
    Stereo::Core::PixelRect whole;
    Stereo::Core::PixelRect empty;

    whole.height = HEIGHT;
    whole.width  = WIDTH;

    for (const Stereo::Core::PixelRect& picture : {whole,
                                                   empty})
    {
        Stereo::ConverterDLL::PixelRowWriter::blank_bars(picture,
                                                         WIDTH,
                                                         HEIGHT,
                                                         0, /* in_eye */
                                                         reinterpret_cast<uint8_t*>(image.data() ),
                                                         WIDTH * 2 * sizeof(uint32_t) );
    }

    for (const uint32_t pixel : image)
    {
        EXPECT_EQ(pixel,
                  WHITE_BGRA);
    }
}

TEST(PixelRowWriterTest, TheAlphaOfOneEyeBecomesOpaqueAndItsColorAndTheOtherEyeStayAsTheyAre)
{
    constexpr uint32_t COLOR_MASK = 0x00FFFFFF;
    constexpr uint32_t FADED_BGRA = 0x02336699; /* alpha 2: what a fade drawn into the alpha of a window leaves */
    constexpr uint32_t HEIGHT     = 3;
    constexpr uint32_t WIDTH      = 5;

    std::vector<uint32_t> image(static_cast<size_t>(WIDTH) * 2 * HEIGHT,
                                FADED_BGRA);

    Stereo::ConverterDLL::PixelRowWriter::make_opaque(WIDTH,
                                                      HEIGHT,
                                                      1, /* in_eye */
                                                      reinterpret_cast<uint8_t*>(image.data() ),
                                                      WIDTH * 2 * sizeof(uint32_t) );

    for (uint32_t n_row = 0;
                  n_row < HEIGHT;
                ++n_row)
    {
        for (uint32_t n_column = 0;
                      n_column < WIDTH;
                    ++n_column)
        {
            const uint32_t left_eye  = image.at(static_cast<size_t>(n_row) * WIDTH * 2 + n_column);
            const uint32_t right_eye = image.at(static_cast<size_t>(n_row) * WIDTH * 2 + WIDTH + n_column);

            EXPECT_EQ(left_eye,
                      FADED_BGRA) << "the other eye is not touched: " << n_row << ", " << n_column;
            EXPECT_EQ(right_eye >> 24,
                      0xFFu) << "the alpha is 255: " << n_row << ", " << n_column;
            EXPECT_EQ(right_eye & COLOR_MASK,
                      FADED_BGRA & COLOR_MASK) << "the color is kept: " << n_row << ", " << n_column;
        }
    }
}
