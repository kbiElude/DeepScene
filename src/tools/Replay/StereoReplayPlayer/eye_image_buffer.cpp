/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "eye_image_buffer.h"
#include <utility>

Stereo::Replay::Player::EyeImageBuffer::EyeImageBuffer(const EyeMode& in_eye_mode)
    :m_eye_mode(in_eye_mode)
{
    /* Stub */
}

void Stereo::Replay::Player::EyeImageBuffer::on_post_failed()
{
    std::lock_guard<std::mutex> lock(m_pending.mutex);

    m_pending.is_woken = false;
}

bool Stereo::Replay::Player::EyeImageBuffer::submit(const uint32_t&                              in_n_execution,
                                                    const Stereo::Replay::EngineDLL::FrameImage& in_image)
{
    const uint32_t n_eye = (m_eye_mode == EyeMode::BOTH) ? in_n_execution
                         : (m_eye_mode == EyeMode::LEFT) ? EYE_LEFT
                                                         : EYE_RIGHT;
    bool           result = false;

    if (n_eye >= N_EYES)
    {
        goto end;
    }

    {
        auto& staging = m_staging_image_array.at(n_eye);

        staging.height = in_image.height;
        staging.width  = in_image.width;

        staging.pixel_u8_vec.assign(in_image.pixel_ptr,
                                    in_image.pixel_ptr + static_cast<size_t>(in_image.width) * in_image.height * sizeof(uint32_t) );
    }

    /* The left image of a pair waits for the right one. */
    if (m_eye_mode == EyeMode::BOTH &&
        n_eye      != EYE_RIGHT)
    {
        goto end;
    }

    {
        std::lock_guard<std::mutex> lock(m_pending.mutex);

        for (uint32_t n_published_eye = 0;
                      n_published_eye < N_EYES;
                    ++n_published_eye)
        {
            const bool is_shown = (m_eye_mode == EyeMode::BOTH ||
                                   n_published_eye == n_eye);

            if (is_shown)
            {
                auto& pending = m_pending.image_array.at(n_published_eye);
                auto& staging = m_staging_image_array.at(n_published_eye);

                pending.height = staging.height;
                pending.width  = staging.width;

                pending.pixel_u8_vec.swap(staging.pixel_u8_vec);

                m_pending.is_new_array.at(n_published_eye) = true;
            }
        }

        result             = (m_pending.is_woken == false);
        m_pending.is_woken = true;
    }

end:
    return result;
}

bool Stereo::Replay::Player::EyeImageBuffer::take(std::array<Image, N_EYES>* inout_image_array_ptr,
                                                  std::array<bool, N_EYES>*  out_is_new_array_ptr)
{
    bool result = false;

    {
        std::lock_guard<std::mutex> lock(m_pending.mutex);

        m_pending.is_woken = false;

        for (uint32_t n_eye = 0;
                      n_eye < N_EYES;
                    ++n_eye)
        {
            const bool is_new = m_pending.is_new_array.at(n_eye);
    
            out_is_new_array_ptr->at(n_eye) = is_new;
    
            if (is_new)
            {
                auto& image   = inout_image_array_ptr->at(n_eye);
                auto& pending = m_pending.image_array.at(n_eye);
    
                image.height = pending.height;
                image.width  = pending.width;
    
                image.pixel_u8_vec.swap(pending.pixel_u8_vec);
    
                m_pending.is_new_array.at(n_eye) = false;
                result                           = true;
            }
        }
    }

    return result;
}
