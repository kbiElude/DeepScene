/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Checks of the images of replayed frames: the PNG and the GIF that are written, how two images are compared, and how images are put together.
 */
#include <gtest/gtest.h>
#include "stereo/replay/frame_images/gif_writer.h"
#include "stereo/replay/frame_images/image_comparer.h"
#include "stereo/replay/frame_images/image_composer.h"
#include "stereo/replay/frame_images/png_writer.h"
#include "miniz.h"
#include <stdlib.h>
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

namespace
{
    /* What a GIF holds, as an independent reader reads it: the size, and for each frame the delay and the colours (R, G, B for each pixel). */
    struct DecodedGif
    {
        std::vector<uint32_t>             delay_vec;   /* in hundredths of a second */
        uint32_t                          height     = 0;
        bool                              is_looping = false;
        std::vector<std::vector<uint8_t>> rgb_u8_vec_vec;
        uint32_t                          width      = 0;
    };

    uint32_t read_u16(const std::vector<uint8_t>& in_byte_vec,
                      const size_t&               in_offset)
    {
        return in_byte_vec.at(in_offset) | (static_cast<uint32_t>(in_byte_vec.at(in_offset + 1) ) << 8);
    }

    /* The LZW of a GIF, read the way a viewer does. False if the codes are not valid. */
    bool decompress_lzw(const std::vector<uint8_t>& in_data_u8_vec,
                        const uint32_t&             in_min_code_size,
                        const size_t&               in_n_pixels,
                        std::vector<uint8_t>*       out_index_u8_vec_ptr)
    {
        const uint32_t clear_code = 1u << in_min_code_size;
        const uint32_t end_code   = clear_code + 1;

        std::vector<int32_t> prefix_vec(4096, -1);
        std::vector<uint8_t> suffix_vec(4096, 0);
        uint32_t             bit_buffer = 0;
        uint32_t             code_size  = in_min_code_size + 1;
        int32_t              last_code  = -1;
        uint32_t             n_bits     = 0;
        size_t               n_byte     = 0;
        uint32_t             next_code  = end_code + 1;
        bool                 result     = false;

        out_index_u8_vec_ptr->clear();

        for (;;)
        {
            while (n_bits < code_size && n_byte < in_data_u8_vec.size() )
            {
                bit_buffer |= static_cast<uint32_t>(in_data_u8_vec.at(n_byte++) ) << n_bits;
                n_bits     += 8;
            }

            if (n_bits < code_size)
            {
                break;
            }

            const uint32_t code = bit_buffer & ((1u << code_size) - 1);

            bit_buffer >>= code_size;
            n_bits      -= code_size;

            if (code == clear_code)
            {
                code_size = in_min_code_size + 1;
                last_code = -1;
                next_code = end_code + 1;

                continue;
            }

            if (code == end_code)
            {
                result = (out_index_u8_vec_ptr->size() == in_n_pixels);

                break;
            }

            std::vector<uint8_t> string_u8_vec; /* back to front */
            int32_t              walk_code = static_cast<int32_t>(code);

            if (code >= next_code)
            {
                /* The string which the decoder has not made yet: the one before it, and the first of that. */
                if (last_code < 0 ||
                    code      >  next_code)
                {
                    break;
                }

                walk_code = last_code;
            }

            while (walk_code >= 0)
            {
                const bool is_literal = (walk_code < static_cast<int32_t>(clear_code) );

                string_u8_vec.push_back((is_literal) ? static_cast<uint8_t>(walk_code)
                                                     : suffix_vec.at(walk_code) );

                walk_code = (is_literal) ? -1
                                         : prefix_vec.at(walk_code);
            }

            if (code >= next_code)
            {
                string_u8_vec.insert(string_u8_vec.begin(),
                                     string_u8_vec.back() );
            }

            for (auto iterator  = string_u8_vec.rbegin();
                      iterator != string_u8_vec.rend();
                    ++iterator)
            {
                out_index_u8_vec_ptr->push_back(*iterator);
            }

            if (last_code >= 0 &&
                next_code <  4096)
            {
                prefix_vec.at(next_code) = last_code;
                suffix_vec.at(next_code) = string_u8_vec.back();

                next_code++;

                if (next_code == (1u << code_size) &&
                    code_size  <  12)
                {
                    code_size++;
                }
            }

            last_code = static_cast<int32_t>(code);
        }

        return result;
    }

    bool decode_gif(const std::vector<uint8_t>& in_file_u8_vec,
                    DecodedGif*                 out_gif_ptr)
    {
        size_t   offset        = 6;
        uint32_t pending_delay = 0;
        bool     result        = false;

        if (in_file_u8_vec.size() < 14 ||
            ::memcmp(in_file_u8_vec.data(),
                     "GIF89a",
                     6) != 0)
        {
            return false;
        }

        out_gif_ptr->width  = read_u16(in_file_u8_vec, offset);
        out_gif_ptr->height = read_u16(in_file_u8_vec, offset + 2);

        /* No global palette: a frame has its own. */
        if ((in_file_u8_vec.at(offset + 4) & 0x80) != 0)
        {
            return false;
        }

        offset += 7;

        while (offset < in_file_u8_vec.size() )
        {
            const uint8_t block = in_file_u8_vec.at(offset++);

            if (block == 0x3B)
            {
                result = true;

                break;
            }

            if (block == 0x21)
            {
                const uint8_t label = in_file_u8_vec.at(offset++);

                if (label == 0xF9)
                {
                    pending_delay = read_u16(in_file_u8_vec, offset + 2);
                }

                if (label == 0xFF &&
                    ::memcmp(in_file_u8_vec.data() + offset + 1,
                             "NETSCAPE2.0",
                             11) == 0)
                {
                    out_gif_ptr->is_looping = (read_u16(in_file_u8_vec, offset + 15) == 0);
                }

                while (in_file_u8_vec.at(offset) != 0)
                {
                    offset += in_file_u8_vec.at(offset) + 1;
                }

                offset++;

                continue;
            }

            if (block != 0x2C)
            {
                return false;
            }

            const uint32_t width  = read_u16(in_file_u8_vec, offset + 4);
            const uint32_t height = read_u16(in_file_u8_vec, offset + 6);
            const uint8_t  packed = in_file_u8_vec.at(offset + 8);

            offset += 9;

            if ((packed & 0x80) == 0)
            {
                return false;
            }

            const size_t         palette_offset = offset;
            std::vector<uint8_t> data_u8_vec;
            std::vector<uint8_t> index_u8_vec;
            std::vector<uint8_t> rgb_u8_vec;

            offset += 256 * 3;

            const uint32_t min_code_size = in_file_u8_vec.at(offset++);

            while (in_file_u8_vec.at(offset) != 0)
            {
                const size_t n_block_bytes = in_file_u8_vec.at(offset++);

                data_u8_vec.insert(data_u8_vec.end(),
                                   in_file_u8_vec.begin() + offset,
                                   in_file_u8_vec.begin() + offset + n_block_bytes);

                offset += n_block_bytes;
            }

            offset++;

            if (decompress_lzw(data_u8_vec,
                               min_code_size,
                               static_cast<size_t>(width) * height,
                              &index_u8_vec) == false)
            {
                return false;
            }

            for (const uint8_t index : index_u8_vec)
            {
                rgb_u8_vec.push_back(in_file_u8_vec.at(palette_offset + index * 3 + 0) );
                rgb_u8_vec.push_back(in_file_u8_vec.at(palette_offset + index * 3 + 1) );
                rgb_u8_vec.push_back(in_file_u8_vec.at(palette_offset + index * 3 + 2) );
            }

            out_gif_ptr->delay_vec.push_back(pending_delay);
            out_gif_ptr->rgb_u8_vec_vec.push_back(rgb_u8_vec);
        }

        return result;
    }

    /* One of 200 colours that a GIF holds as they are: each channel a multiple of 32, and no two in one bin of 5 bits of each channel. */
    void get_exact_color(const uint32_t& in_n_color,
                         uint8_t*        out_blue_ptr,
                         uint8_t*        out_green_ptr,
                         uint8_t*        out_red_ptr)
    {
        *out_red_ptr   = static_cast<uint8_t>((in_n_color % 8) * 32);
        *out_green_ptr = static_cast<uint8_t>(((in_n_color / 8) % 8) * 32);
        *out_blue_ptr  = static_cast<uint8_t>((in_n_color / 64) * 32);
    }
}

TEST(GifWriter, FramesOfFewColoursComeBackExactlyWithTheirDelayAndPlayForEver)
{
    auto                 first_image  = make_image(5, 3, 0, 0, 0);
    auto                 second_image = make_image(5, 3, 128, 64, 32);
    std::vector<uint8_t> file_u8_vec;
    DecodedGif           gif;

    set_pixel(&first_image,  1, 1, 255, 0, 255); /* magenta */
    set_pixel(&second_image, 4, 2, 255, 0, 255);
    set_pixel(&second_image, 0, 0, 200, 200, 200);

    ASSERT_TRUE(Stereo::Replay::FrameImages::GifWriter::encode({first_image, second_image},
                                                               700,
                                                              &file_u8_vec) );
    ASSERT_TRUE(decode_gif(file_u8_vec,
                          &gif) );

    EXPECT_EQ(gif.width,
              5u);
    EXPECT_EQ(gif.height,
              3u);
    EXPECT_TRUE(gif.is_looping);
    ASSERT_EQ(gif.rgb_u8_vec_vec.size(),
              2u);
    EXPECT_EQ(gif.delay_vec.at(0),
              70u);
    EXPECT_EQ(gif.delay_vec.at(1),
              70u);

    /* A colour that no other is near is as it was. */
    for (size_t n_frame = 0;
                n_frame < 2;
              ++n_frame)
    {
        const auto& image = (n_frame == 0) ? first_image
                                           : second_image;

        for (size_t n_pixel = 0;
                    n_pixel < 15;
                  ++n_pixel)
        {
            EXPECT_EQ(gif.rgb_u8_vec_vec.at(n_frame).at(n_pixel * 3 + 0),
                      image.pixel_u8_vec.at(n_pixel * N_BYTES_PER_PIXEL + 2) ) << n_frame << " " << n_pixel;
            EXPECT_EQ(gif.rgb_u8_vec_vec.at(n_frame).at(n_pixel * 3 + 1),
                      image.pixel_u8_vec.at(n_pixel * N_BYTES_PER_PIXEL + 1) );
            EXPECT_EQ(gif.rgb_u8_vec_vec.at(n_frame).at(n_pixel * 3 + 2),
                      image.pixel_u8_vec.at(n_pixel * N_BYTES_PER_PIXEL + 0) );
        }
    }
}

TEST(GifWriter, ManyColoursInNoOrderFillTheTableOfCodesAndStillComeBackExactly)
{
    constexpr uint32_t HEIGHT   = 300;
    constexpr uint32_t N_COLORS = 200;
    constexpr uint32_t WIDTH    = 300;

    auto                 image = make_image(WIDTH, HEIGHT, 0, 0, 0);
    std::vector<uint8_t> file_u8_vec;
    DecodedGif           gif;
    size_t               n_different = 0;
    uint32_t             seed        = 12345;

    /* 90,000 pixels of 200 colours in an order which repeats nothing for long: the codes run out and the table starts over, a few times. */
    for (uint32_t n_y = 0;
                  n_y < HEIGHT;
                ++n_y)
    {
        for (uint32_t n_x = 0;
                      n_x < WIDTH;
                    ++n_x)
        {
            uint8_t blue  = 0;
            uint8_t green = 0;
            uint8_t red   = 0;

            seed = seed * 1664525u + 1013904223u;

            get_exact_color((seed >> 16) % N_COLORS,
                           &blue,
                           &green,
                           &red);
            set_pixel(&image,
                      n_x,
                      n_y,
                      blue,
                      green,
                      red);
        }
    }

    ASSERT_TRUE(Stereo::Replay::FrameImages::GifWriter::encode({image},
                                                               100,
                                                              &file_u8_vec) );
    ASSERT_TRUE(decode_gif(file_u8_vec,
                          &gif) );
    ASSERT_EQ(gif.rgb_u8_vec_vec.size(),
              1u);
    ASSERT_EQ(gif.rgb_u8_vec_vec.at(0).size(),
              static_cast<size_t>(WIDTH) * HEIGHT * 3);

    for (size_t n_pixel = 0;
                n_pixel < static_cast<size_t>(WIDTH) * HEIGHT;
              ++n_pixel)
    {
        const bool is_same = (gif.rgb_u8_vec_vec.at(0).at(n_pixel * 3 + 0) == image.pixel_u8_vec.at(n_pixel * N_BYTES_PER_PIXEL + 2) &&
                              gif.rgb_u8_vec_vec.at(0).at(n_pixel * 3 + 1) == image.pixel_u8_vec.at(n_pixel * N_BYTES_PER_PIXEL + 1) &&
                              gif.rgb_u8_vec_vec.at(0).at(n_pixel * 3 + 2) == image.pixel_u8_vec.at(n_pixel * N_BYTES_PER_PIXEL + 0) );

        n_different += (is_same) ? 0
                                 : 1;
    }

    EXPECT_EQ(n_different,
              0u);
}

TEST(GifWriter, MoreThan256ColoursAreKeptNearTheirOwn)
{
    constexpr uint32_t HEIGHT = 64;
    constexpr uint32_t WIDTH  = 256;

    auto                 image = make_image(WIDTH, HEIGHT, 0, 0, 0);
    std::vector<uint8_t> file_u8_vec;
    DecodedGif           gif;
    double               sum_error = 0.0;

    for (uint32_t n_y = 0;
                  n_y < HEIGHT;
                ++n_y)
    {
        for (uint32_t n_x = 0;
                      n_x < WIDTH;
                    ++n_x)
        {
            set_pixel(&image,
                      n_x,
                      n_y,
                      static_cast<uint8_t>((n_x + n_y) / 2),
                      static_cast<uint8_t>(n_y * 4),
                      static_cast<uint8_t>(n_x) );
        }
    }

    ASSERT_TRUE(Stereo::Replay::FrameImages::GifWriter::encode({image},
                                                               100,
                                                              &file_u8_vec) );
    ASSERT_TRUE(decode_gif(file_u8_vec,
                          &gif) );

    for (size_t n_pixel = 0;
                n_pixel < static_cast<size_t>(WIDTH) * HEIGHT;
              ++n_pixel)
    {
        for (size_t n_channel = 0;
                    n_channel < 3;
                  ++n_channel)
        {
            sum_error += std::abs(static_cast<int>(gif.rgb_u8_vec_vec.at(0).at(n_pixel * 3 + n_channel) ) -
                                  static_cast<int>(image.pixel_u8_vec.at(n_pixel * N_BYTES_PER_PIXEL + 2 - n_channel) ) );
        }
    }

    /* 16,384 pixels in 256 colours, from a smooth ramp: the mean error of a channel is a few steps of 255. */
    EXPECT_LT(sum_error / (static_cast<double>(WIDTH) * HEIGHT * 3),
              6.0);
}

TEST(GifWriter, NoFramesOrFramesOfDifferentSizesOrEmptyOnesAreNotWritten)
{
    std::vector<uint8_t> file_u8_vec;

    EXPECT_FALSE(Stereo::Replay::FrameImages::GifWriter::encode({},
                                                                100,
                                                               &file_u8_vec) );
    EXPECT_FALSE(Stereo::Replay::FrameImages::GifWriter::encode({make_image(4, 4, 0, 0, 0),
                                                                 make_image(4, 5, 0, 0, 0)},
                                                                100,
                                                               &file_u8_vec) );
    EXPECT_FALSE(Stereo::Replay::FrameImages::GifWriter::encode({Stereo::Replay::FrameImages::Image()},
                                                                100,
                                                               &file_u8_vec) );
}
