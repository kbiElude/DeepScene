/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "utils/images/png_writer.h"
#include "miniz.h"
#include <stdio.h>

bool Utils::Images::PngWriter::encode(const Image&          in_image,
                                      std::vector<uint8_t>* out_file_u8_vec_ptr)
{
    constexpr size_t   N_BYTES_PER_PIXEL_IN  = 4; /* B, G, R, A */
    constexpr int      N_CHANNELS_OUT        = 3; /* R, G, B    */
    constexpr mz_uint  COMPRESSION_LEVEL     = 6;

    void*                file_ptr     = nullptr;
    size_t               n_file_bytes = 0;
    const size_t         n_pixels     = static_cast<size_t>(in_image.width) * in_image.height;
    std::vector<uint8_t> rgb_u8_vec;
    bool                 result       = false;

    out_file_u8_vec_ptr->clear();

    if (n_pixels                     == 0                               ||
        in_image.pixel_u8_vec.size() <  n_pixels * N_BYTES_PER_PIXEL_IN)
    {
        goto end;
    }

    rgb_u8_vec.resize(n_pixels * N_CHANNELS_OUT);

    for (size_t n_pixel = 0;
                n_pixel < n_pixels;
              ++n_pixel)
    {
        const uint8_t* const pixel_ptr = in_image.pixel_u8_vec.data() + n_pixel * N_BYTES_PER_PIXEL_IN;
        uint8_t* const       out_ptr   = rgb_u8_vec.data()            + n_pixel * N_CHANNELS_OUT;

        out_ptr[0] = pixel_ptr[2];
        out_ptr[1] = pixel_ptr[1];
        out_ptr[2] = pixel_ptr[0];
    }

    file_ptr = ::tdefl_write_image_to_png_file_in_memory_ex(rgb_u8_vec.data (),
                                                            static_cast<int>(in_image.width),
                                                            static_cast<int>(in_image.height),
                                                            N_CHANNELS_OUT,
                                                           &n_file_bytes,
                                                            COMPRESSION_LEVEL,
                                                            MZ_FALSE); /* flip */

    if (file_ptr == nullptr)
    {
        goto end;
    }

    out_file_u8_vec_ptr->assign(static_cast<const uint8_t*>(file_ptr),
                                static_cast<const uint8_t*>(file_ptr) + n_file_bytes);

    ::mz_free(file_ptr);

    result = true;
end:
    return result;
}

bool Utils::Images::PngWriter::write_file(const std::string& in_path,
                                          const Image&       in_image)
{
    std::vector<uint8_t> file_u8_vec;
    FILE*                file_handle = nullptr;
    bool                 is_written  = false;
    bool                 result      = false;

    if (encode(in_image,
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

    {
        const int close_result = ::fclose(file_handle);

        result = (close_result == 0 &&
                  is_written);
    }

end:
    return result;
}
