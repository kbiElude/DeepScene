/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Tests for the cache that serves the reads of one pixel of a picture on the graphics card: when the picture is read, when what was read is let go, and which pixel is given.
 */
#include <gtest/gtest.h>
#include "stereo/core/picture_readback_cache.h"

namespace
{
    constexpr uint32_t PICTURE_HEIGHT = 3;
    constexpr uint32_t PICTURE_WIDTH  = 4;

    /* A picture whose pixel at (x, y) is x, y, x + y and 255, so that what is given tells where it was read from. */
    bool fill_picture(std::vector<uint8_t>* out_rgba_u8_vec_ptr)
    {
        out_rgba_u8_vec_ptr->clear();

        for (uint32_t y = 0;
                      y < PICTURE_HEIGHT;
                    ++y)
        {
            for (uint32_t x = 0;
                          x < PICTURE_WIDTH;
                        ++x)
            {
                out_rgba_u8_vec_ptr->push_back(static_cast<uint8_t>(x) );
                out_rgba_u8_vec_ptr->push_back(static_cast<uint8_t>(y) );
                out_rgba_u8_vec_ptr->push_back(static_cast<uint8_t>(x + y) );
                out_rgba_u8_vec_ptr->push_back(255);
            }
        }

        return true;
    }

    bool read_pixel_of(Stereo::Core::PictureReadbackCache* inout_cache_ptr,
                       const Stereo::Core::PictureReadbackCache::FillFunc& in_fill_func,
                       const int64_t&                                      in_x,
                       const int64_t&                                      in_y,
                       std::array<uint8_t, 4>*                             out_rgba_ptr)
    {
        return inout_cache_ptr->read_pixel(in_fill_func,
                                           PICTURE_WIDTH,
                                           PICTURE_HEIGHT,
                                           in_x,
                                           in_y,
                                           out_rgba_ptr);
    }
}

TEST(PictureReadbackCacheTest, TheFirstReadOfARunIsMadeAsItIsAndTheSecondReadsThePicture)
{
    Stereo::Core::PictureReadbackCache cache;
    uint32_t                           n_fills = 0;
    std::array<uint8_t, 4>             rgba    = {};

    const auto fill_func = [&n_fills](std::vector<uint8_t>* out_rgba_u8_vec_ptr)
    {
        n_fills++;

        return fill_picture(out_rgba_u8_vec_ptr);
    };

    EXPECT_FALSE(read_pixel_of(&cache, fill_func, 1, 2, &rgba) );
    EXPECT_EQ   (n_fills,
                 0u);

    EXPECT_TRUE (read_pixel_of(&cache, fill_func, 1, 2, &rgba) );
    EXPECT_EQ   (n_fills,
                 1u);
    EXPECT_EQ   (rgba.at(0),
                 1);
    EXPECT_EQ   (rgba.at(1),
                 2);
    EXPECT_EQ   (rgba.at(2),
                 3);
    EXPECT_EQ   (rgba.at(3),
                 255);
}

TEST(PictureReadbackCacheTest, TheRestOfTheRunIsGivenFromThePictureThatWasRead)
{
    Stereo::Core::PictureReadbackCache cache;
    uint32_t                           n_fills = 0;
    std::array<uint8_t, 4>             rgba    = {};

    const auto fill_func = [&n_fills](std::vector<uint8_t>* out_rgba_u8_vec_ptr)
    {
        n_fills++;

        return fill_picture(out_rgba_u8_vec_ptr);
    };

    read_pixel_of(&cache, fill_func, 0, 0, &rgba);

    for (int64_t n_read = 0;
                 n_read < 1000;
               ++n_read)
    {
        EXPECT_TRUE(read_pixel_of(&cache, fill_func, n_read % PICTURE_WIDTH, n_read % PICTURE_HEIGHT, &rgba) );
        EXPECT_EQ  (rgba.at(0),
                    n_read % PICTURE_WIDTH);
        EXPECT_EQ  (rgba.at(1),
                    n_read % PICTURE_HEIGHT);
    }

    EXPECT_EQ(n_fills,
              1u);
}

TEST(PictureReadbackCacheTest, ACallThatMayChangeThePictureEndsTheRun)
{
    Stereo::Core::PictureReadbackCache cache;
    uint32_t                           n_fills = 0;
    std::array<uint8_t, 4>             rgba    = {};

    const auto fill_func = [&n_fills](std::vector<uint8_t>* out_rgba_u8_vec_ptr)
    {
        n_fills++;

        return fill_picture(out_rgba_u8_vec_ptr);
    };

    read_pixel_of(&cache, fill_func, 0, 0, &rgba);
    read_pixel_of(&cache, fill_func, 0, 0, &rgba);

    cache.invalidate();

    /* The next run begins as the first did: a read that is made as it is, then the picture is read again. */
    EXPECT_FALSE(read_pixel_of(&cache, fill_func, 0, 0, &rgba) );
    EXPECT_TRUE (read_pixel_of(&cache, fill_func, 0, 0, &rgba) );
    EXPECT_EQ   (n_fills,
                 2u);
}

TEST(PictureReadbackCacheTest, APixelThatIsOutsideOfThePictureIsReadAtTheNearestOneItHas)
{
    Stereo::Core::PictureReadbackCache cache;
    std::array<uint8_t, 4>             rgba = {};

    read_pixel_of(&cache, fill_picture, 0, 0, &rgba);

    /* Gerbera reads at -13 and at 653 of 640, and at 480 of 480. */
    ASSERT_TRUE(read_pixel_of(&cache, fill_picture, -13, -2, &rgba) );
    EXPECT_EQ  (rgba.at(0),
                0);
    EXPECT_EQ  (rgba.at(1),
                0);

    ASSERT_TRUE(read_pixel_of(&cache, fill_picture, 653, PICTURE_HEIGHT, &rgba) );
    EXPECT_EQ  (rgba.at(0),
                PICTURE_WIDTH - 1);
    EXPECT_EQ  (rgba.at(1),
                PICTURE_HEIGHT - 1);
}

TEST(PictureReadbackCacheTest, APictureThatCannotBeReadIsNotTriedAgainInTheRun)
{
    Stereo::Core::PictureReadbackCache cache;
    uint32_t                           n_fills = 0;
    std::array<uint8_t, 4>             rgba    = {};

    const auto fail_func = [&n_fills](std::vector<uint8_t>* out_rgba_u8_vec_ptr)
    {
        (void) out_rgba_u8_vec_ptr;

        n_fills++;

        return false;
    };

    for (int n_read = 0;
             n_read < 10;
           ++n_read)
    {
        EXPECT_FALSE(read_pixel_of(&cache, fail_func, 1, 1, &rgba) );
    }

    EXPECT_EQ(n_fills,
              1u);

    /* The next run tries again. */
    cache.invalidate();

    read_pixel_of(&cache, fail_func, 1, 1, &rgba);
    read_pixel_of(&cache, fail_func, 1, 1, &rgba);

    EXPECT_EQ(n_fills,
              2u);
}

TEST(PictureReadbackCacheTest, APictureOfAnotherSizeIsNotTheOneThatIsKept)
{
    Stereo::Core::PictureReadbackCache cache;
    uint32_t                           n_fills = 0;
    std::array<uint8_t, 4>             rgba    = {};

    const auto fill_func = [&n_fills](std::vector<uint8_t>* out_rgba_u8_vec_ptr)
    {
        n_fills++;

        return fill_picture(out_rgba_u8_vec_ptr);
    };

    read_pixel_of(&cache, fill_func, 0, 0, &rgba);
    read_pixel_of(&cache, fill_func, 0, 0, &rgba);

    /* The picture is made again for the size that the application has now, which is read as the application's reads come. */
    EXPECT_TRUE(cache.read_pixel(fill_func,
                                 PICTURE_WIDTH,
                                 PICTURE_HEIGHT - 1,
                                 0,
                                 0,
                                &rgba) );
    EXPECT_EQ  (n_fills,
                2u);
}

TEST(PictureReadbackCacheTest, AFillThatGivesTooFewPixelsIsNotUsed)
{
    Stereo::Core::PictureReadbackCache cache;
    std::array<uint8_t, 4>             rgba = {};

    const auto short_func = [](std::vector<uint8_t>* out_rgba_u8_vec_ptr)
    {
        out_rgba_u8_vec_ptr->assign(8,
                                    0);

        return true;
    };

    read_pixel_of(&cache, short_func, 0, 0, &rgba);

    EXPECT_FALSE(read_pixel_of(&cache, short_func, 0, 0, &rgba) );
}

TEST(PictureReadbackCacheTest, ThePictureWithNoSizeIsNeverRead)
{
    Stereo::Core::PictureReadbackCache cache;
    std::array<uint8_t, 4>             rgba = {};

    EXPECT_FALSE(cache.read_pixel(fill_picture,
                                  0,
                                  PICTURE_HEIGHT,
                                  0,
                                  0,
                                 &rgba) );
    EXPECT_FALSE(cache.read_pixel(fill_picture,
                                  PICTURE_WIDTH,
                                  0,
                                  0,
                                  0,
                                 &rgba) );
}
