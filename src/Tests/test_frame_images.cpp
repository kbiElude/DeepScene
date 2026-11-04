/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Checks of the images of replayed frames: the PNG that is written, how two images are compared, and how images are put together.
 */
#include <gtest/gtest.h>
#include "Stereo/Replay/FrameImages/image_comparer.h"
#include "Stereo/Replay/FrameImages/image_composer.h"
#include "Stereo/Replay/FrameImages/png_writer.h"
#include "miniz.h"
#include <string.h>

namespace
{
    constexpr size_t N_BYTES_PER_PIXEL = 4;

    /* An image of one color (B, G, R; opaque). */
    Stereo::Replay::FrameImages::Image make_image(const uint32_t& in_width,
                                                  const uint32_t& in_height,
                                                  const uint8_t&  in_blue,
                                                  const uint8_t&  in_green,
                                                  const uint8_t&  in_red)
    {
        Stereo::Replay::FrameImages::Image result;

        result.height = in_height;
        result.width  = in_width;

        for (size_t n_pixel = 0;
                    n_pixel < static_cast<size_t>(in_width) * in_height;
                  ++n_pixel)
        {
            result.pixel_u8_vec.push_back(in_blue);
            result.pixel_u8_vec.push_back(in_green);
            result.pixel_u8_vec.push_back(in_red);
            result.pixel_u8_vec.push_back(255);
        }

        return result;
    }

    /* Sets the pixel to the color (B, G, R). */
    void set_pixel(Stereo::Replay::FrameImages::Image* out_image_ptr,
                   const uint32_t&                     in_x,
                   const uint32_t&                     in_y,
                   const uint8_t&                      in_blue,
                   const uint8_t&                      in_green,
                   const uint8_t&                      in_red)
    {
        uint8_t* const pixel_ptr = out_image_ptr->pixel_u8_vec.data() + (static_cast<size_t>(in_y) * out_image_ptr->width + in_x) * N_BYTES_PER_PIXEL;

        pixel_ptr[0] = in_blue;
        pixel_ptr[1] = in_green;
        pixel_ptr[2] = in_red;
    }
}

TEST(PngWriter, AnImageIsAPngWithItsSizeAndItsPixelsInRgbOrder)
{
    constexpr uint32_t HEIGHT         = 2;
    constexpr uint32_t WIDTH          = 3;
    constexpr size_t   IHDR_OFFSET    = 16; /* the signature (8), the length and the name of the first chunk (8) */
    constexpr size_t   N_SIGNATURE    = 8;

    const uint8_t signature_u8vec8[N_SIGNATURE] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};

    Stereo::Replay::FrameImages::Image image = make_image(WIDTH,
                                                          HEIGHT,
                                                          10,
                                                          20,
                                                          30);
    std::vector<uint8_t>               file_u8_vec;

    set_pixel(&image,
              2,
              1,
              1,
              2,
              3);

    ASSERT_TRUE(Stereo::Replay::FrameImages::PngWriter::encode(image,
                                                              &file_u8_vec) );
    ASSERT_GT(file_u8_vec.size(),
              static_cast<size_t>(IHDR_OFFSET + 8) );
    EXPECT_EQ(::memcmp(file_u8_vec.data(),
                       signature_u8vec8,
                       N_SIGNATURE),
              0);

    /* The width and the height are big-endian in the header chunk. */
    EXPECT_EQ(file_u8_vec.at(IHDR_OFFSET + 3),
              WIDTH);
    EXPECT_EQ(file_u8_vec.at(IHDR_OFFSET + 7),
              HEIGHT);

    /* The IDAT chunk follows the header chunk (25 bytes) and holds the zlib stream. */
    constexpr size_t IDAT_OFFSET = N_SIGNATURE + 25;

    const uint32_t idat_length = (static_cast<uint32_t>(file_u8_vec.at(IDAT_OFFSET + 0) ) << 24) |
                                 (static_cast<uint32_t>(file_u8_vec.at(IDAT_OFFSET + 1) ) << 16) |
                                 (static_cast<uint32_t>(file_u8_vec.at(IDAT_OFFSET + 2) ) << 8)  |
                                  static_cast<uint32_t>(file_u8_vec.at(IDAT_OFFSET + 3) );

    ASSERT_EQ(::memcmp(file_u8_vec.data() + IDAT_OFFSET + 4,
                       "IDAT",
                       4),
              0);

    std::vector<uint8_t> row_u8_vec((1 + WIDTH * 3) * HEIGHT);
    mz_ulong             n_row_bytes = static_cast<mz_ulong>(row_u8_vec.size() );

    ASSERT_EQ(::mz_uncompress(row_u8_vec.data(),
                             &n_row_bytes,
                              file_u8_vec.data() + IDAT_OFFSET + 8,
                              idat_length),
              MZ_OK);
    ASSERT_EQ(n_row_bytes,
              row_u8_vec.size() );

    /* The first pixel (B 10, G 20, R 30) and the last one of the second row (B 1, G 2, R 3). */
    EXPECT_EQ(row_u8_vec.at(1),
              30);
    EXPECT_EQ(row_u8_vec.at(2),
              20);
    EXPECT_EQ(row_u8_vec.at(3),
              10);
    EXPECT_EQ(row_u8_vec.at(row_u8_vec.size() - 3),
              3);
    EXPECT_EQ(row_u8_vec.at(row_u8_vec.size() - 2),
              2);
    EXPECT_EQ(row_u8_vec.at(row_u8_vec.size() - 1),
              1);
}

TEST(PngWriter, AnEmptyImageOrOneWithTooFewPixelsIsNotWritten)
{
    Stereo::Replay::FrameImages::Image image = make_image(4,
                                                          4,
                                                          0,
                                                          0,
                                                          0);
    std::vector<uint8_t>               file_u8_vec;

    EXPECT_FALSE(Stereo::Replay::FrameImages::PngWriter::encode(Stereo::Replay::FrameImages::Image(),
                                                               &file_u8_vec) );

    image.pixel_u8_vec.resize(image.pixel_u8_vec.size() - 1);

    EXPECT_FALSE(Stereo::Replay::FrameImages::PngWriter::encode(image,
                                                               &file_u8_vec) );
    EXPECT_TRUE(file_u8_vec.empty() );
}

TEST(ImageComparer, ImagesThatAreTheSameDoNotDiffer)
{
    const auto image = make_image(5,
                                  4,
                                  1,
                                  2,
                                  3);
    const auto result = Stereo::Replay::FrameImages::ImageComparer::compare(image,
                                                                            image);

    EXPECT_TRUE(result.has_same_size);
    EXPECT_EQ(result.n_differing_pixels,
              0u);
    EXPECT_EQ(result.max_channel_difference,
              0u);
}

TEST(ImageComparer, ThePixelsThatDifferAreCountedAndTheirBoundsAndLargestStepAreSaid)
{
    auto       first_image  = make_image(8,
                                        6,
                                        100,
                                        100,
                                        100);
    auto       second_image = first_image;

    set_pixel(&second_image,
              2,
              1,
              100,
              100,
              103);
    set_pixel(&second_image,
              6,
              4,
              90,
              100,
              100);

    const auto result = Stereo::Replay::FrameImages::ImageComparer::compare(first_image,
                                                                            second_image);

    EXPECT_EQ(result.n_differing_pixels,
              2u);
    EXPECT_EQ(result.max_channel_difference,
              10u);
    EXPECT_EQ(result.bounding_box_min_x,
              2u);
    EXPECT_EQ(result.bounding_box_min_y,
              1u);
    EXPECT_EQ(result.bounding_box_max_x,
              6u);
    EXPECT_EQ(result.bounding_box_max_y,
              4u);
}

TEST(ImageComparer, ImagesOfOtherSizesDifferInEverythingAndHaveNoDifferenceImage)
{
    const auto                          first_image  = make_image(4, 4, 0, 0, 0);
    const auto                          second_image = make_image(4, 5, 0, 0, 0);
    Stereo::Replay::FrameImages::Image difference_image;

    EXPECT_FALSE(Stereo::Replay::FrameImages::ImageComparer::compare(first_image,
                                                                     second_image).has_same_size);
    EXPECT_FALSE(Stereo::Replay::FrameImages::ImageComparer::make_difference_image(first_image,
                                                                                   second_image,
                                                                                  &difference_image) );
    EXPECT_EQ(difference_image.width,
              0u);
}

TEST(ImageComparer, TheDifferenceImageIsRedWhereThePixelsDifferAndBlackElsewhere)
{
    const auto                          first_image  = make_image(3, 2, 7, 7, 7);
    auto                                second_image = first_image;
    Stereo::Replay::FrameImages::Image difference_image;

    set_pixel(&second_image,
              1,
              1,
              7,
              8,
              7);

    ASSERT_TRUE(Stereo::Replay::FrameImages::ImageComparer::make_difference_image(first_image,
                                                                                  second_image,
                                                                                 &difference_image) );
    ASSERT_EQ(difference_image.width,
              3u);
    ASSERT_EQ(difference_image.height,
              2u);

    /* (1, 1) differs: pixel 4 of 6. */
    const size_t different_offset = 4 * N_BYTES_PER_PIXEL;

    EXPECT_EQ(difference_image.pixel_u8_vec.at(different_offset + 2),
              255);
    EXPECT_EQ(difference_image.pixel_u8_vec.at(different_offset + 0),
              0);
    EXPECT_EQ(difference_image.pixel_u8_vec.at(2),
              0);
}

TEST(ImageComposer, AnAnaglyphHasTheRedOfTheLeftImageAndTheRestOfTheRightOne)
{
    const auto                          left_image  = make_image(2, 2, 11, 12, 13);
    const auto                          right_image = make_image(2, 2, 21, 22, 23);
    Stereo::Replay::FrameImages::Image anaglyph_image;

    ASSERT_TRUE(Stereo::Replay::FrameImages::ImageComposer::make_anaglyph(left_image,
                                                                          right_image,
                                                                         &anaglyph_image) );
    ASSERT_EQ(anaglyph_image.pixel_u8_vec.size(),
              4 * N_BYTES_PER_PIXEL);
    EXPECT_EQ(anaglyph_image.pixel_u8_vec.at(0),
              21);
    EXPECT_EQ(anaglyph_image.pixel_u8_vec.at(1),
              22);
    EXPECT_EQ(anaglyph_image.pixel_u8_vec.at(2),
              13);
    EXPECT_EQ(anaglyph_image.pixel_u8_vec.at(3),
              255);

    EXPECT_FALSE(Stereo::Replay::FrameImages::ImageComposer::make_anaglyph(left_image,
                                                                           make_image(3, 2, 0, 0, 0),
                                                                          &anaglyph_image) );
}

TEST(ImageComposer, AContactSheetHasImagesInRowsWithGapsAndEmptySpaceFilled)
{
    constexpr uint32_t GAP = 4;

    std::vector<Stereo::Replay::FrameImages::Image> image_vec;
    Stereo::Replay::FrameImages::Image              sheet_image;

    image_vec.push_back(make_image(3, 2, 1, 1, 1) );
    image_vec.push_back(make_image(2, 3, 2, 2, 2) );
    image_vec.push_back(make_image(3, 3, 3, 3, 3) );

    ASSERT_TRUE(Stereo::Replay::FrameImages::ImageComposer::make_contact_sheet(image_vec,
                                                                               2,
                                                                              &sheet_image) );

    /* Two columns of cells 3 wide, two rows of cells 3 high. */
    EXPECT_EQ(sheet_image.width,
              2 * 3 + GAP);
    EXPECT_EQ(sheet_image.height,
              2 * 3 + GAP);

    /* The first image is at the top left, the second at the top of the second column, the third in the second row. */
    EXPECT_EQ(sheet_image.pixel_u8_vec.at(0),
              1);
    EXPECT_EQ(sheet_image.pixel_u8_vec.at((0 * sheet_image.width + 3 + GAP) * N_BYTES_PER_PIXEL),
              2);
    EXPECT_EQ(sheet_image.pixel_u8_vec.at(((3 + GAP) * sheet_image.width) * N_BYTES_PER_PIXEL),
              3);

    /* The gap, and the part of a cell which its image does not cover, are the background. */
    EXPECT_EQ(sheet_image.pixel_u8_vec.at(3 * N_BYTES_PER_PIXEL),
              40);
    EXPECT_EQ(sheet_image.pixel_u8_vec.at(((2) * sheet_image.width) * N_BYTES_PER_PIXEL),
              40);
}

TEST(ImageComposer, NoImagesNoColumnsOrAnEmptyImageGiveNoSheet)
{
    std::vector<Stereo::Replay::FrameImages::Image> image_vec;
    Stereo::Replay::FrameImages::Image              sheet_image;

    EXPECT_FALSE(Stereo::Replay::FrameImages::ImageComposer::make_contact_sheet(image_vec,
                                                                                1,
                                                                               &sheet_image) );

    image_vec.push_back(make_image(2, 2, 0, 0, 0) );

    EXPECT_FALSE(Stereo::Replay::FrameImages::ImageComposer::make_contact_sheet(image_vec,
                                                                                0,
                                                                               &sheet_image) );

    image_vec.push_back(Stereo::Replay::FrameImages::Image() );

    EXPECT_FALSE(Stereo::Replay::FrameImages::ImageComposer::make_contact_sheet(image_vec,
                                                                                2,
                                                                               &sheet_image) );
}

TEST(ImageComposer, AnEyeViewIsOneEyeBothSideBySideOrAnAnaglyph)
{
    const auto                         left_image  = make_image(2, 2, 11, 12, 13);
    const auto                         right_image = make_image(2, 2, 21, 22, 23);
    Stereo::Replay::FrameImages::Image view_image;

    ASSERT_TRUE(Stereo::Replay::FrameImages::ImageComposer::make_eye_view(Stereo::Replay::FrameImages::EyeView::LEFT,
                                                                          left_image,
                                                                          right_image,
                                                                         &view_image) );
    EXPECT_EQ(view_image.pixel_u8_vec.at(0),
              11);

    ASSERT_TRUE(Stereo::Replay::FrameImages::ImageComposer::make_eye_view(Stereo::Replay::FrameImages::EyeView::RIGHT,
                                                                          left_image,
                                                                          right_image,
                                                                         &view_image) );
    EXPECT_EQ(view_image.pixel_u8_vec.at(0),
              21);

    /* Side by side: two images of 2 pixels and the gap between them. */
    ASSERT_TRUE(Stereo::Replay::FrameImages::ImageComposer::make_eye_view(Stereo::Replay::FrameImages::EyeView::BOTH,
                                                                          left_image,
                                                                          right_image,
                                                                         &view_image) );
    EXPECT_EQ(view_image.width,
              2 + 4 + 2u);
    EXPECT_EQ(view_image.height,
              2u);

    ASSERT_TRUE(Stereo::Replay::FrameImages::ImageComposer::make_eye_view(Stereo::Replay::FrameImages::EyeView::ANAGLYPH,
                                                                          left_image,
                                                                          right_image,
                                                                         &view_image) );
    EXPECT_EQ(view_image.pixel_u8_vec.at(2),
              13);
    EXPECT_EQ(view_image.pixel_u8_vec.at(0),
              21);
}

TEST(ImageComposer, AnEyeViewOfAnEmptyEyeIsNotMade)
{
    Stereo::Replay::FrameImages::Image view_image;

    EXPECT_FALSE(Stereo::Replay::FrameImages::ImageComposer::make_eye_view(Stereo::Replay::FrameImages::EyeView::RIGHT,
                                                                           make_image(2, 2, 1, 1, 1),
                                                                           Stereo::Replay::FrameImages::Image(),
                                                                          &view_image) );
    EXPECT_EQ(view_image.width,
              0u);
}
