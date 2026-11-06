/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "utils/images/gif_writer.h"
#include <algorithm>
#include <stdio.h>
#include <string.h>

namespace
{
    /* A box of the median cut: the colours it holds (as 15 bits: 5 of each of R, G and B), and how many pixels have them. */
    struct ColorBox
    {
        std::vector<uint16_t> color_vec;
        uint64_t              n_pixels  = 0;
    };

    void append_u16(const uint32_t&       in_value,
                    std::vector<uint8_t>* inout_byte_vec_ptr)
    {
        inout_byte_vec_ptr->push_back(static_cast<uint8_t>(in_value & 0xFF) );
        inout_byte_vec_ptr->push_back(static_cast<uint8_t>((in_value >> 8) & 0xFF) );
    }
}

bool Utils::Images::GifWriter::encode(const std::vector<Image>& in_frame_vec,
                                      const uint32_t&           in_frame_delay_n_milliseconds,
                                      std::vector<uint8_t>*     out_file_u8_vec_ptr)
{
    constexpr size_t   N_BYTES_PER_PIXEL       = 4;     /* B, G, R, A                                         */
    constexpr uint32_t MAX_EXTENT              = 65535;
    constexpr uint32_t N_MILLISECONDS_PER_UNIT = 10;    /* a delay is in hundredths of a second               */
    constexpr size_t   N_PALETTE_BYTES         = 768;
    constexpr uint8_t  PACKED_LOGICAL_SCREEN   = 0x70;  /* no global palette; 8 bits of colour                */
    constexpr uint8_t  PACKED_FRAME            = 0x87;  /* a local palette of 256 colours                     */
    constexpr uint8_t  PACKED_FRAME_CONTROL    = 0x04;  /* the frame is left as it is when the next is drawn  */

    bool result = false;

    out_file_u8_vec_ptr->clear();

    if (in_frame_vec.empty() )
    {
        goto end;
    }

    for (const auto& frame : in_frame_vec)
    {
        if (frame.width                != in_frame_vec.front().width  ||
            frame.height               != in_frame_vec.front().height ||
            frame.width                == 0                           ||
            frame.height               == 0                           ||
            frame.width                >  MAX_EXTENT                  ||
            frame.height               >  MAX_EXTENT                  ||
            frame.pixel_u8_vec.size()  <  static_cast<size_t>(frame.width) * frame.height * N_BYTES_PER_PIXEL)
        {
            goto end;
        }
    }

    {
        const std::string header_text = "GIF89a";

        out_file_u8_vec_ptr->insert(out_file_u8_vec_ptr->end(),
                                    header_text.begin(),
                                    header_text.end  () );

        append_u16(in_frame_vec.front().width,
                   out_file_u8_vec_ptr);
        append_u16(in_frame_vec.front().height,
                   out_file_u8_vec_ptr);

        out_file_u8_vec_ptr->push_back(PACKED_LOGICAL_SCREEN);
        out_file_u8_vec_ptr->push_back(0); /* the colour of the background  */
        out_file_u8_vec_ptr->push_back(0); /* the aspect ratio: not given   */

        /* The extension that makes it play for ever. */
        const uint8_t loop_u8vec19[] = {0x21, 0xFF, 0x0B, 'N', 'E', 'T', 'S', 'C', 'A', 'P', 'E', '2', '.', '0', 0x03, 0x01, 0x00, 0x00, 0x00};

        out_file_u8_vec_ptr->insert(out_file_u8_vec_ptr->end(),
                                    loop_u8vec19,
                                    loop_u8vec19 + sizeof(loop_u8vec19) );
    }

    for (const auto& frame : in_frame_vec)
    {
        std::vector<uint8_t>     index_u8_vec;
        std::array<uint8_t, 768> palette_u8vec768;

        quantize(frame,
                &palette_u8vec768,
                &index_u8_vec);

        out_file_u8_vec_ptr->push_back(0x21); /* a graphic control extension: the delay */
        out_file_u8_vec_ptr->push_back(0xF9);
        out_file_u8_vec_ptr->push_back(0x04);
        out_file_u8_vec_ptr->push_back(PACKED_FRAME_CONTROL);

        append_u16(std::max<uint32_t>(1,
                                      in_frame_delay_n_milliseconds / N_MILLISECONDS_PER_UNIT),
                   out_file_u8_vec_ptr);

        out_file_u8_vec_ptr->push_back(0); /* the colour that is see-through: none */
        out_file_u8_vec_ptr->push_back(0);

        out_file_u8_vec_ptr->push_back(0x2C); /* the frame, from the top left corner */

        append_u16(0,
                   out_file_u8_vec_ptr);
        append_u16(0,
                   out_file_u8_vec_ptr);
        append_u16(frame.width,
                   out_file_u8_vec_ptr);
        append_u16(frame.height,
                   out_file_u8_vec_ptr);

        out_file_u8_vec_ptr->push_back(PACKED_FRAME);
        out_file_u8_vec_ptr->insert(out_file_u8_vec_ptr->end(),
                                    palette_u8vec768.begin(),
                                    palette_u8vec768.begin() + N_PALETTE_BYTES);

        compress_lzw(index_u8_vec,
                     out_file_u8_vec_ptr);
    }

    out_file_u8_vec_ptr->push_back(0x3B);

    result = true;
end:
    return result;
}

bool Utils::Images::GifWriter::write_file(const std::string&        in_path,
                                          const std::vector<Image>& in_frame_vec,
                                          const uint32_t&           in_frame_delay_n_milliseconds)
{
    std::vector<uint8_t> file_u8_vec;
    FILE*                file_handle = nullptr;
    bool                 is_written  = false;
    bool                 result      = false;

    if (encode(in_frame_vec,
               in_frame_delay_n_milliseconds,
              &file_u8_vec) == false)
    {
        goto end;
    }

    file_handle = ::fopen(in_path.c_str(),
                          "wb");

    if (file_handle == nullptr)
    {
        goto end;
    }

    is_written = (::fwrite(file_u8_vec.data(),
                           file_u8_vec.size(),
                           1,
                           file_handle) == 1);
    result     = (::fclose(file_handle) == 0 &&
                  is_written);

end:
    return result;
}

void Utils::Images::GifWriter::compress_lzw(const std::vector<uint8_t>& in_index_u8_vec,
                                            std::vector<uint8_t>*       inout_file_u8_vec_ptr)
{
    constexpr uint32_t CLEAR_CODE          = 256;
    constexpr uint32_t END_CODE            = 257;
    constexpr uint32_t FIRST_FREE_CODE     = 258;
    constexpr uint32_t INITIAL_CODE_SIZE   = 9;
    constexpr size_t   MAX_BLOCK_N_BYTES   = 255;
    constexpr uint32_t MAX_CODE_SIZE       = 12;
    constexpr uint32_t MAX_N_CODES         = 1u << MAX_CODE_SIZE;
    constexpr uint8_t  MIN_CODE_SIZE       = 8;
    constexpr size_t   N_SYMBOLS           = 256;

    uint32_t             bit_buffer  = 0;
    uint32_t             code_size   = INITIAL_CODE_SIZE;
    std::vector<uint8_t> data_u8_vec;
    uint32_t             n_bits      = 0;
    uint32_t             next_code   = FIRST_FREE_CODE;
    int32_t              prefix_code = -1; /* none yet */
  std::vector<int16_t>  table        (static_cast<size_t>(MAX_N_CODES) * N_SYMBOLS,
                                      -1);

    const auto emit_func = [&](const uint32_t& in_code)
    {
        bit_buffer |= (in_code << n_bits);
        n_bits     += code_size;

        while (n_bits >= 8)
        {
            data_u8_vec.push_back(static_cast<uint8_t>(bit_buffer & 0xFF) );

            bit_buffer >>= 8;
            n_bits      -= 8;
        }
    };

    /* The codes are one bit longer as soon as the next one to be made does not fit in the length they have. */
    const auto grow_func = [&]()
    {
        if (next_code  > (1u << code_size) - 1 &&
            code_size  < MAX_CODE_SIZE)
        {
            code_size++;
        }
    };

    emit_func(CLEAR_CODE);

    for (const uint8_t index : in_index_u8_vec)
    {
        if (prefix_code < 0)
        {
            prefix_code = index;

            continue;
        }

        const size_t key = static_cast<size_t>(prefix_code) * N_SYMBOLS + index;

        if (table.at(key) >= 0)
        {
            prefix_code = table.at(key);

            continue;
        }

        emit_func(static_cast<uint32_t>(prefix_code) );
        grow_func();

        if (next_code < MAX_N_CODES)
        {
            table.at(key) = static_cast<int16_t>(next_code);
            next_code++;
        }
        else
        {
            /* The table is full: it starts over. */
            emit_func(CLEAR_CODE);

            std::fill(table.begin(),
                      table.end  (),
                      static_cast<int16_t>(-1) );

            code_size = INITIAL_CODE_SIZE;
            next_code = FIRST_FREE_CODE;
        }

        prefix_code = index;
    }

    if (prefix_code >= 0)
    {
        emit_func(static_cast<uint32_t>(prefix_code) );
        grow_func();
    }

    emit_func(END_CODE);

    if (n_bits > 0)
    {
        data_u8_vec.push_back(static_cast<uint8_t>(bit_buffer & 0xFF) );
    }

    inout_file_u8_vec_ptr->push_back(MIN_CODE_SIZE);

    for (size_t n_byte = 0;
                n_byte < data_u8_vec.size();
                n_byte += MAX_BLOCK_N_BYTES)
    {
        const size_t n_block_bytes = std::min<size_t>(MAX_BLOCK_N_BYTES,
                                                      data_u8_vec.size() - n_byte);

        inout_file_u8_vec_ptr->push_back(static_cast<uint8_t>(n_block_bytes) );
        inout_file_u8_vec_ptr->insert(inout_file_u8_vec_ptr->end(),
                                      data_u8_vec.begin() + n_byte,
                                      data_u8_vec.begin() + n_byte + n_block_bytes);
    }

    inout_file_u8_vec_ptr->push_back(0); /* the end of the blocks */
}

void Utils::Images::GifWriter::quantize(const Image&              in_image,
                                        std::array<uint8_t, 768>* out_palette_u8vec768_ptr,
                                        std::vector<uint8_t>*     out_index_u8_vec_ptr)
{
    constexpr size_t   N_BYTES_PER_PIXEL = 4; /* B, G, R, A */
    constexpr size_t   N_COLORS          = 256;
    constexpr size_t   N_BINS            = 1u << 15;
    constexpr uint32_t BIN_SHIFT         = 3; /* 8 bits to 5 */
    constexpr uint32_t CHANNEL_MASK      = 31;

    const size_t                n_pixels = static_cast<size_t>(in_image.width) * in_image.height;
    std::vector<uint8_t>        box_of_bin_u8_vec(N_BINS,
                                                  0);
    std::vector<ColorBox>       box_vec(1);
    std::vector<uint32_t>       count_u32_vec(N_BINS,
                                              0);
    std::vector<uint32_t>       sum_b_u32_vec (N_BINS,
                                               0);
    std::vector<uint32_t>       sum_g_u32_vec (N_BINS,
                                               0);
    std::vector<uint32_t>       sum_r_u32_vec (N_BINS,
                                               0);

    out_palette_u8vec768_ptr->fill(0);

    for (size_t n_pixel = 0;
                n_pixel < n_pixels;
              ++n_pixel)
    {
        const uint8_t* const pixel_ptr = in_image.pixel_u8_vec.data() + n_pixel * N_BYTES_PER_PIXEL;
        const size_t         bin       = (static_cast<size_t>(pixel_ptr[2] >> BIN_SHIFT) << 10) |
                                         (static_cast<size_t>(pixel_ptr[1] >> BIN_SHIFT) << 5)  |
                                          static_cast<size_t>(pixel_ptr[0] >> BIN_SHIFT);

        count_u32_vec.at(bin)++;
        sum_b_u32_vec.at(bin) += pixel_ptr[0];
        sum_g_u32_vec.at(bin) += pixel_ptr[1];
        sum_r_u32_vec.at(bin) += pixel_ptr[2];
    }

    for (size_t n_bin = 0;
                n_bin < N_BINS;
              ++n_bin)
    {
        if (count_u32_vec.at(n_bin) != 0)
        {
            box_vec.front().color_vec.push_back(static_cast<uint16_t>(n_bin) );
            box_vec.front().n_pixels += count_u32_vec.at(n_bin);
        }
    }

    /* The box of most pixels which has more than one colour is cut in two at the median of the pixels, along the channel in which its colours are furthest apart. */
    while (box_vec.size() < N_COLORS)
    {
        size_t n_best_box = box_vec.size(); /* none */

        for (size_t n_box = 0;
                    n_box < box_vec.size();
                  ++n_box)
        {
            if (box_vec.at(n_box).color_vec.size() > 1                                                                  &&
                (n_best_box == box_vec.size() || box_vec.at(n_box).n_pixels > box_vec.at(n_best_box).n_pixels) )
            {
                n_best_box = n_box;
            }
        }

        if (n_best_box == box_vec.size() )
        {
            break;
        }

        ColorBox      new_box;
        uint32_t      max_channel_vec[3] = {0, 0, 0};
        uint32_t      min_channel_vec[3] = {CHANNEL_MASK, CHANNEL_MASK, CHANNEL_MASK};
        size_t   n_axis         = 0;
        uint64_t n_pixels_below = 0;
        size_t   n_split        = 0;

        {
            ColorBox& box = box_vec.at(n_best_box);

            for (const uint16_t color : box.color_vec)
            {
                const uint32_t channel_vec[3] = {static_cast<uint32_t>(color >> 10),
                                                 static_cast<uint32_t>((color >> 5) & CHANNEL_MASK),
                                                 static_cast<uint32_t>(color & CHANNEL_MASK)};

                for (size_t n_channel = 0;
                            n_channel < 3;
                          ++n_channel)
                {
                    max_channel_vec[n_channel] = std::max(max_channel_vec[n_channel],
                                                          channel_vec[n_channel]);
                    min_channel_vec[n_channel] = std::min(min_channel_vec[n_channel],
                                                          channel_vec[n_channel]);
                }
            }

            for (size_t n_channel = 1;
                        n_channel < 3;
                      ++n_channel)
            {
                if (max_channel_vec[n_channel] - min_channel_vec[n_channel] > max_channel_vec[n_axis] - min_channel_vec[n_axis])
                {
                    n_axis = n_channel;
                }
            }

            const uint32_t shift = (n_axis == 0) ? 10
                                 : (n_axis == 1) ? 5
                                                 : 0;

            std::sort(box.color_vec.begin(),
                      box.color_vec.end  (),
                      [shift, CHANNEL_MASK](const uint16_t& in_color_a,
                                            const uint16_t& in_color_b)
                      {
                          return (((in_color_a >> shift) & CHANNEL_MASK) < ((in_color_b >> shift) & CHANNEL_MASK) );
                      });

            while (n_split + 1 < box.color_vec.size() &&
                   n_pixels_below * 2 < box.n_pixels)
            {
                n_pixels_below += count_u32_vec.at(box.color_vec.at(n_split) );
                n_split++;
            }

            new_box.color_vec.assign(box.color_vec.begin() + n_split,
                                     box.color_vec.end  () );
            box.color_vec.resize(n_split);

            new_box.n_pixels = box.n_pixels - n_pixels_below;
            box.n_pixels     = n_pixels_below;
        }

        box_vec.push_back(new_box);
    }

    for (size_t n_box = 0;
                n_box < box_vec.size();
              ++n_box)
    {
        const auto& box   = box_vec.at(n_box);
        uint64_t    sum_b = 0;
        uint64_t    sum_g = 0;
        uint64_t    sum_r = 0;

        for (const uint16_t color : box.color_vec)
        {
            sum_b += sum_b_u32_vec.at(color);
            sum_g += sum_g_u32_vec.at(color);
            sum_r += sum_r_u32_vec.at(color);

            box_of_bin_u8_vec.at(color) = static_cast<uint8_t>(n_box);
        }

        if (box.n_pixels != 0)
        {
            out_palette_u8vec768_ptr->at(n_box * 3 + 0) = static_cast<uint8_t>((sum_r + box.n_pixels / 2) / box.n_pixels);
            out_palette_u8vec768_ptr->at(n_box * 3 + 1) = static_cast<uint8_t>((sum_g + box.n_pixels / 2) / box.n_pixels);
            out_palette_u8vec768_ptr->at(n_box * 3 + 2) = static_cast<uint8_t>((sum_b + box.n_pixels / 2) / box.n_pixels);
        }
    }

    out_index_u8_vec_ptr->resize(n_pixels);

    for (size_t n_pixel = 0;
                n_pixel < n_pixels;
              ++n_pixel)
    {
        const uint8_t* const pixel_ptr = in_image.pixel_u8_vec.data() + n_pixel * N_BYTES_PER_PIXEL;
        const size_t         bin       = (static_cast<size_t>(pixel_ptr[2] >> BIN_SHIFT) << 10) |
                                         (static_cast<size_t>(pixel_ptr[1] >> BIN_SHIFT) << 5)  |
                                          static_cast<size_t>(pixel_ptr[0] >> BIN_SHIFT);

        out_index_u8_vec_ptr->at(n_pixel) = box_of_bin_u8_vec.at(bin);
    }
}
