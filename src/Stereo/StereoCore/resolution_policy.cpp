/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "Stereo/StereoCore/resolution_policy.h"
#include <math.h>

Stereo::StereoCore::ResolutionPolicy::ResolutionPolicy(const float& in_scale)
    :m_scale((::isfinite(in_scale) == false          || in_scale < get_min_scale() ) ? get_min_scale()
            : (in_scale            > get_max_scale() )                               ? get_max_scale()
                                                                                     : in_scale)
{
    /* Stub */
}

float Stereo::StereoCore::ResolutionPolicy::get_fit_scale(const uint32_t& in_target_width,
                                                          const uint32_t& in_target_height,
                                                          const uint32_t& in_width,
                                                          const uint32_t& in_height)
{
    float result = 1.0f;

    if (in_target_width  > 0 &&
        in_target_height > 0 &&
        in_width         > 0 &&
        in_height        > 0)
    {
        const double width_scale  = static_cast<double>(in_target_width)  / static_cast<double>(in_width);
        const double height_scale = static_cast<double>(in_target_height) / static_cast<double>(in_height);

        result = static_cast<float>((width_scale < height_scale) ? width_scale
                                                                 : height_scale);
    }

    return result;
}

float Stereo::StereoCore::ResolutionPolicy::get_max_scale()
{
    return 4.0f;
}

float Stereo::StereoCore::ResolutionPolicy::get_min_scale()
{
    return 1.0f;
}

float Stereo::StereoCore::ResolutionPolicy::get_scale() const
{
    return m_scale;
}

bool Stereo::StereoCore::ResolutionPolicy::is_identity() const
{
    return (m_scale == get_min_scale() );
}

uint32_t Stereo::StereoCore::ResolutionPolicy::scale_extent(const uint32_t& in_extent) const
{
    return static_cast<uint32_t>(::floor(static_cast<double>(in_extent) * static_cast<double>(m_scale) + 0.5) );
}
