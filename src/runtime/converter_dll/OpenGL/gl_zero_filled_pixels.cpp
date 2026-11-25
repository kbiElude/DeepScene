/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "gl_zero_filled_pixels.h"
#include <string.h>

size_t Stereo::ConverterDLL::GLZeroFilledPixels::get_n_scaled_bytes(const uint32_t& in_n_source_bytes,
                                                                    const uint64_t& in_n_source_pixels,
                                                                    const uint32_t& in_scaled_width,
                                                                    const uint32_t& in_scaled_height)
{
    constexpr size_t MAX_UNPACK_ALIGNMENT = 8;

    size_t result = 0;

    if (in_n_source_bytes  == 0 ||
        in_n_source_pixels == 0)
    {
        goto end;
    }

    {
        const size_t n_bytes_per_pixel = static_cast<size_t>( (in_n_source_bytes + in_n_source_pixels - 1) / in_n_source_pixels);

        result = static_cast<size_t>(in_scaled_height) * (static_cast<size_t>(in_scaled_width) * n_bytes_per_pixel + MAX_UNPACK_ALIGNMENT);
    }

end:
    return result;
}

bool Stereo::ConverterDLL::GLZeroFilledPixels::is_zero_filled(const void*     in_data_ptr,
                                                              const uint32_t& in_n_bytes)
{
    bool result = false;

    if (in_data_ptr == nullptr ||
        in_n_bytes  == 0)
    {
        goto end;
    }

    {
        const uint8_t* bytes_ptr = static_cast<const uint8_t*>(in_data_ptr);

        /* The first byte is zero, and every byte equals the one before it. */
        result = (bytes_ptr[0] == 0 &&
                  ::memcmp(bytes_ptr,
                           bytes_ptr + 1,
                           in_n_bytes - 1) == 0);
    }

end:
    return result;
}
