/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "depth_buffer_statistics.h"
#include <algorithm>
#include <math.h>
#include <vector>

/* A depth at or above this value was not drawn to (the buffer is cleared to 1). */
const float Stereo::Core::DepthBufferStatistics::NOT_DRAWN_DEPTH = 0.999999f;

float Stereo::Core::DepthBufferStatistics::get_distance(const float& in_depth,
                                                        const float& in_projection_z_scale,
                                                        const float& in_projection_z_shift)
{
    /* A depth maps to the normalized device z = 2 * depth - 1. For this projection z_device = -scale - shift / z_eye, and the camera looks
     * down -z, so the distance is shift / (z_device + scale).
     */
    const float divisor = 2.0f * in_depth - 1.0f + in_projection_z_scale;
    const float result  = (divisor != 0.0f) ? in_projection_z_shift / divisor
                                            : 0.0f;

    return (::isfinite(result) && result > 0.0f) ? result
                                                 : 0.0f;
}

Stereo::Core::DepthSelection Stereo::Core::DepthBufferStatistics::get_selection(const size_t& in_n_drawn,
                                                                                const size_t& in_n_looked_at,
                                                                                const size_t& in_n_sky,
                                                                                const float&  in_projection_z_scale,
                                                                                const float&  in_projection_z_shift,
                                                                                const float&  in_fraction)
{
    /* Fewer drawn pixels than MIN_N_DRAWN, or fewer than one drawn pixel per N_LOOKED_AT_PER_DRAWN examined pixels, is too little to measure. */
    constexpr size_t MIN_N_DRAWN           = 16;
    constexpr size_t N_LOOKED_AT_PER_DRAWN = 100;

    DepthSelection result;

    /* A buffer with nothing or hardly anything drawn in it does not say how deep the scene is. */
    if (in_n_drawn >= MIN_N_DRAWN                           &&
        in_n_drawn >= in_n_looked_at / N_LOOKED_AT_PER_DRAWN)
    {
        result.far_plane = get_distance(1.0f,
                                        in_projection_z_scale,
                                        in_projection_z_shift);
        result.has_sky   = (result.far_plane              >  0.0f                                                                   &&
                            static_cast<double>(in_n_sky) >= static_cast<double>(in_n_looked_at) * static_cast<double>(in_fraction) );
        result.is_enough = true;
        result.n_edge    = std::min(in_n_drawn - 1,
                                    static_cast<size_t>(static_cast<double>(in_n_drawn) * static_cast<double>(in_fraction) ) );
    }

    return result;
}

Stereo::Core::DepthRange Stereo::Core::DepthBufferStatistics::measure(const float*  in_depth_ptr,
                                                                      const size_t& in_n_depths,
                                                                      const size_t& in_step,
                                                                      const float&  in_projection_z_scale,
                                                                      const float&  in_projection_z_shift,
                                                                      const float&  in_fraction)
{
    std::vector<float> distance_vec;
    size_t             n_looked_at = 0;
    size_t             n_sky       = 0;
    DepthRange         result;
    const size_t       step        = (in_step > 0) ? in_step
                                                   : 1;

    distance_vec.reserve(in_n_depths / step + 1);

    for (size_t n_depth = 0;
                n_depth < in_n_depths;
                n_depth += step)
    {
        n_looked_at++;

        if (in_depth_ptr[n_depth] >= NOT_DRAWN_DEPTH)
        {
            n_sky++;
        }
        else
        {
            const float distance = get_distance(in_depth_ptr[n_depth],
                                                in_projection_z_scale,
                                                in_projection_z_shift);

            if (distance > 0.0f)
            {
                distance_vec.push_back(distance);
            }
        }
    }

    const size_t         n_drawn   = distance_vec.size();
    const DepthSelection selection = get_selection(n_drawn,
                                                   n_looked_at,
                                                   n_sky,
                                                   in_projection_z_scale,
                                                   in_projection_z_shift,
                                                   in_fraction);

    if (selection.is_enough)
    {
        std::nth_element(distance_vec.begin(),
                         distance_vec.begin() + static_cast<std::ptrdiff_t>(selection.n_edge),
                         distance_vec.end  () );

        result.near_depth = distance_vec.at(selection.n_edge);

        std::nth_element(distance_vec.begin(),
                         distance_vec.begin() + static_cast<std::ptrdiff_t>(n_drawn - 1 - selection.n_edge),
                         distance_vec.end  () );

        result.far_depth = (selection.has_sky) ? selection.far_plane
                                               : distance_vec.at(n_drawn - 1 - selection.n_edge);

        std::nth_element(distance_vec.begin(),
                         distance_vec.begin() + static_cast<std::ptrdiff_t>(n_drawn / 2),
                         distance_vec.end  () );

        result.median_depth = distance_vec.at(n_drawn / 2);
        result.has_depth    = (result.far_depth > result.near_depth);
    }

    return result;
}
