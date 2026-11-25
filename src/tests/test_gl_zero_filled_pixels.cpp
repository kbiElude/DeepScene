/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Pixels that are all zero: how the converter tells them from pixels with content, and how large a buffer of zeros the upload of a larger texture reads.
 */
#include <gtest/gtest.h>
#include "OpenGL/gl_zero_filled_pixels.h"
#include <vector>

TEST(GLZeroFilledPixels, BytesThatAreAllZeroAreZeroFilled)
{
    const std::vector<uint8_t> zero_u8_vec(1000,
                                           0);

    EXPECT_TRUE(Stereo::ConverterDLL::GLZeroFilledPixels::is_zero_filled(zero_u8_vec.data(),
                                                                         static_cast<uint32_t>(zero_u8_vec.size() ) ) );
}

TEST(GLZeroFilledPixels, OneByteThatIsNotZeroMakesThePixelsHaveContent)
{
    std::vector<uint8_t> pixel_u8_vec(1000,
                                      0);

    /* At the start, in the middle and at the end: wherever the byte is, it is found. */
    for (const size_t position : {size_t(0), size_t(499), size_t(999)})
    {
        pixel_u8_vec.at(position) = 1;

        EXPECT_FALSE(Stereo::ConverterDLL::GLZeroFilledPixels::is_zero_filled(pixel_u8_vec.data(),
                                                                              static_cast<uint32_t>(pixel_u8_vec.size() ) ) ) << position;

        pixel_u8_vec.at(position) = 0;
    }
}

TEST(GLZeroFilledPixels, NoBytesAreNotZeroFilled)
{
    const uint8_t byte = 0;

    EXPECT_FALSE(Stereo::ConverterDLL::GLZeroFilledPixels::is_zero_filled(nullptr,
                                                                          16) );
    EXPECT_FALSE(Stereo::ConverterDLL::GLZeroFilledPixels::is_zero_filled(&byte,
                                                                          0) );
}

TEST(GLZeroFilledPixels, AllTheBytesOfOnePixelAreZeroToo)
{
    const uint8_t byte = 0;

    EXPECT_TRUE(Stereo::ConverterDLL::GLZeroFilledPixels::is_zero_filled(&byte,
                                                                         1) );
}

TEST(GLZeroFilledPixels, TheBufferOfAScaledUploadHasTheBytesOfThePixelsAndTheRowsOfTheScaledSize)
{
    /* 512 x 256 pixels of 4 bytes, uploaded at twice the size: 1024 rows of 1024 pixels, each row with room to be padded. */
    const size_t n_scaled_bytes = Stereo::ConverterDLL::GLZeroFilledPixels::get_n_scaled_bytes(512 * 256 * 4,
                                                                                               512 * 256,
                                                                                               1024,
                                                                                               512);

    EXPECT_GE(n_scaled_bytes,
              static_cast<size_t>(1024) * 512 * 4);
    EXPECT_LE(n_scaled_bytes,
              static_cast<size_t>(1024) * 512 * 4 + 512 * 8);
}

TEST(GLZeroFilledPixels, ThreeBytePixelsWithPaddedRowsStillFitTheScaledBuffer)
{
    /* 5 x 2 pixels of 3 bytes, with rows padded to 4: 16 bytes a row. A pixel then counts as 4 bytes, so the scaled buffer is never too small. */
    const size_t n_scaled_bytes = Stereo::ConverterDLL::GLZeroFilledPixels::get_n_scaled_bytes(32,
                                                                                               10,
                                                                                               10,
                                                                                               4);
    const size_t n_needed_bytes = 4 * (4 * ((10 * 3 + 3) / 4) );

    EXPECT_GE(n_scaled_bytes,
              n_needed_bytes);
}

TEST(GLZeroFilledPixels, AnUploadWithNoPixelsNeedsNoBuffer)
{
    EXPECT_EQ(Stereo::ConverterDLL::GLZeroFilledPixels::get_n_scaled_bytes(0,
                                                                           10,
                                                                           20,
                                                                           20),
              0u);
    EXPECT_EQ(Stereo::ConverterDLL::GLZeroFilledPixels::get_n_scaled_bytes(64,
                                                                           0,
                                                                           20,
                                                                           20),
              0u);
}
