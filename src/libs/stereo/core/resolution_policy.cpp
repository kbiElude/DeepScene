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

uint32_t Stereo::Core::ResolutionPolicy::scale_pixel_coordinate(const int64_t&  in_coordinate,
                                                                const uint32_t& in_asked_extent,
                                                                const uint32_t& in_real_extent) const
{
    uint32_t result = 0;

    if (in_asked_extent != 0 &&
        in_real_extent  != 0)
    {
        const int64_t last_asked_pixel = static_cast<int64_t>(in_asked_extent) - 1;
        const double  last_real_pixel  = static_cast<double> (in_real_extent)  - 1.0;

        /* The coordinate is moved inside the application's picture first, so that the result is the one that the nearest pixel inside the
         * picture gets.
         */
        const int64_t asked_pixel = (in_coordinate < 0)                ? 0
                                  : (in_coordinate > last_asked_pixel) ? last_asked_pixel
                                                                       : in_coordinate;
        const double  real_pixel  = ::floor( (static_cast<double>(asked_pixel) + 0.5) * static_cast<double>(m_scale) );

        result = static_cast<uint32_t>( (real_pixel > last_real_pixel) ? last_real_pixel
                                                                       : real_pixel);
    }

    return result;
}

Stereo::Core::PixelRect Stereo::Core::ResolutionPolicy::scale_rect(const PixelRect& in_rect) const
{
    PixelRect result;

    result.height = scale_extent(in_rect.height);
    result.width  = scale_extent(in_rect.width);
    result.x      = scale_extent(in_rect.x);
    result.y      = scale_extent(in_rect.y);

    return result;
}

Stereo::Core::PixelRect Stereo::Core::ResolutionPolicy::scale_rect(const PixelRect& in_rect,
                                                                   const uint32_t&  in_bounds_width,
                                                                   const uint32_t&  in_bounds_height) const
{
    PixelRect result = scale_rect(in_rect);

    /* Rounding must not push the rectangle outside the bounds. */
    if (result.x + result.width > in_bounds_width)
    {
        result.width = (result.x < in_bounds_width) ? in_bounds_width - result.x
                                                    : 0;
    }

    if (result.y + result.height > in_bounds_height)
    {
        result.height = (result.y < in_bounds_height) ? in_bounds_height - result.y
                                                      : 0;
    }

    return result;
}
