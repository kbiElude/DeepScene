/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "Stereo/Core/resolution_policy.h"
#include "Stereo/Transport/frame_channel.h"
#include "eye_image_limits.h"

bool Stereo::ConverterDLL::EyeImageLimits::fits(const uint32_t& in_width,
                                                const uint32_t& in_height)
{
    return (in_width  <= get_max_width() &&
            in_height <= get_max_height() );
}

float Stereo::ConverterDLL::EyeImageLimits::get_largest_scale(const uint32_t& in_width,
                                                              const uint32_t& in_height)
{
    return Stereo::Core::ResolutionPolicy::get_fit_scale(get_max_width(),
                                                         get_max_height(),
                                                         in_width,
                                                         in_height);
}

uint32_t Stereo::ConverterDLL::EyeImageLimits::get_max_height()
{
    return Stereo::Transport::FrameChannel::get_max_height();
}

uint32_t Stereo::ConverterDLL::EyeImageLimits::get_max_width()
{
    return Stereo::Transport::FrameChannel::get_max_width() / N_EYES;
}
