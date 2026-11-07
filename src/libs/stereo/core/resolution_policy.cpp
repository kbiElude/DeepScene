/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "stereo/core/resolution_policy.h"
#include <math.h>

Stereo::Core::ResolutionPolicy::ResolutionPolicy(const float& in_scale)
    :m_scale(get_min_scale() )
{
    const bool  is_finite = ::isfinite(in_scale);
    const float max_scale = get_max_scale();
    const float min_scale = get_min_scale();

    if (is_finite           &&
        in_scale >= min_scale)
    {
        m_scale = (in_scale > max_scale) ? max_scale
                                         : in_scale;
    }
}

float Stereo::Core::ResolutionPolicy::get_fit_scale(const uint32_t& in_target_width,
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

float Stereo::Core::ResolutionPolicy::get_max_scale()
{
    return 4.0f;
}

float Stereo::Core::ResolutionPolicy::get_min_scale()
{
    return 1.0f;
}

float Stereo::Core::ResolutionPolicy::get_scale() const
{
    return m_scale;
}

bool Stereo::Core::ResolutionPolicy::is_identity() const
{
    return (m_scale == get_min_scale() );
}

uint32_t Stereo::Core::ResolutionPolicy::scale_extent(const uint32_t& in_extent) const
{
    return static_cast<uint32_t>(::floor(static_cast<double>(in_extent) * static_cast<double>(m_scale) + 0.5) );
}
