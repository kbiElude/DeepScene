/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "stereo/core/picture_readback_cache.h"
#include <algorithm>

Stereo::Core::PictureReadbackCache::PictureReadbackCache()
    :m_has_failed(false),
     m_height    (0),
     m_is_valid  (false),
     m_n_reads   (0),
     m_width     (0)
{
    /* Stub */
}

void Stereo::Core::PictureReadbackCache::invalidate()
{
    m_has_failed = false;
    m_is_valid   = false;
    m_n_reads    = 0;
}

bool Stereo::Core::PictureReadbackCache::read_pixel(const FillFunc&         in_fill_func,
                                                    const uint32_t&         in_picture_width,
                                                    const uint32_t&         in_picture_height,
                                                    const int64_t&          in_x,
                                                    const int64_t&          in_y,
                                                    std::array<uint8_t, 4>* out_rgba_u8vec4_ptr)
{
    /* The whole picture is read at this read of a run. Earlier reads are made directly, so a lone read costs what it always did. */
    constexpr uint32_t N_READS_BEFORE_PICTURE = 2;
    constexpr size_t   N_CHANNELS             = 4;

    bool result = false;

    if (in_picture_width  != 0 &&
        in_picture_height != 0)
    {
        const size_t n_picture_bytes = static_cast<size_t>(in_picture_width) * in_picture_height * N_CHANNELS;

        /* A picture of another size invalidates the stored one. */
        if (m_is_valid                     &&
            (m_width  != in_picture_width  ||
             m_height != in_picture_height) )
        {
            m_is_valid = false;
        }

        m_n_reads++;

        if (m_is_valid   == false                  &&
            m_has_failed == false                  &&
            m_n_reads    >= N_READS_BEFORE_PICTURE)
        {
            const bool is_filled = in_fill_func(&m_rgba_u8_vec);

            m_has_failed = (is_filled == false);
            m_height     = in_picture_height;
            m_is_valid   = (is_filled                               &&
                            m_rgba_u8_vec.size() >= n_picture_bytes);
            m_width      = in_picture_width;
        }

        if (m_is_valid)
        {
            const int64_t last_x = static_cast<int64_t>(in_picture_width)  - 1;
            const int64_t last_y = static_cast<int64_t>(in_picture_height) - 1;

            /* A pixel outside the picture is replaced by the nearest pixel inside it. */
            const int64_t x      = std::min<int64_t>(std::max<int64_t>(in_x,
                                                                       0),
                                                     last_x);
            const int64_t y      = std::min<int64_t>(std::max<int64_t>(in_y,
                                                                       0),
                                                     last_y);
            const size_t  offset = (static_cast<size_t>(y) * in_picture_width + static_cast<size_t>(x) ) * N_CHANNELS;

            for (size_t n_channel = 0;
                        n_channel < N_CHANNELS;
                      ++n_channel)
            {
                out_rgba_u8vec4_ptr->at(n_channel) = m_rgba_u8_vec.at(offset + n_channel);
            }

            result = true;
        }
    }

    return result;
}
