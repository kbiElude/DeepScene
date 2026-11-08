/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "depth_buffer_statistics.h"
#include <algorithm>
#include <math.h>
#include <vector>

Stereo::Core::DepthRange Stereo::Core::DepthBufferStatistics::measure(const float*  in_depth_ptr,
                                                                      const size_t& in_n_depths,
                                                                      const size_t& in_step,
                                                                      const float&  in_projection_z_scale,
                                                                      const float&  in_projection_z_shift,
                                                                      const float&  in_fraction)
{
    /* A depth that is this close to the end of the buffer was not drawn to (the buffer is cleared to 1). */
    constexpr float NOT_DRAWN = 0.999999f;

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

        if (in_depth_ptr[n_depth] >= NOT_DRAWN)
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

    /* A buffer with nothing drawn in it, or hardly anything, says nothing of how deep the scene is. */
    if (distance_vec.size() >= 16                                   &&
        distance_vec.size() >= n_looked_at / 100)
    {
        const size_t n_drawn        = distance_vec.size();
        const size_t n_edge         = std::min(n_drawn - 1,
                                               static_cast<size_t>(static_cast<double>(n_drawn) * static_cast<double>(in_fraction) ) );
        const float  far_plane      = get_distance(1.0f,
                                                   in_projection_z_scale,
                                                   in_projection_z_shift);
        const bool   has_sky        = (far_plane > 0.0f                                                                                                &&
                                       static_cast<double>(n_sky) >= static_cast<double>(n_looked_at) * static_cast<double>(in_fraction) );

        std::nth_element(distance_vec.begin(),
                         distance_vec.begin() + static_cast<std::ptrdiff_t>(n_edge),
                         distance_vec.end  () );

        result.near_depth = distance_vec.at(n_edge);

        std::nth_element(distance_vec.begin(),
                         distance_vec.begin() + static_cast<std::ptrdiff_t>(n_drawn - 1 - n_edge),
                         distance_vec.end  () );

        result.far_depth = (has_sky) ? far_plane
                                     : distance_vec.at(n_drawn - 1 - n_edge);

        std::nth_element(distance_vec.begin(),
                         distance_vec.begin() + static_cast<std::ptrdiff_t>(n_drawn / 2),
                         distance_vec.end  () );

        result.median_depth = distance_vec.at(n_drawn / 2);
        result.has_depth    = (result.far_depth > result.near_depth);
    }

    return result;
}

float Stereo::Core::DepthBufferStatistics::get_distance(const float& in_depth,
                                                        const float& in_projection_z_scale,
                                                        const float& in_projection_z_shift)
{
    /* z of the normalized device = 2 * depth - 1 = -scale - shift / z of the eye, and the camera looks down minus z: the distance is shift / (z of the device + scale). */
    const float divisor = 2.0f * in_depth - 1.0f + in_projection_z_scale;
    const float result  = (divisor != 0.0f) ? in_projection_z_shift / divisor
                                            : 0.0f;

    return (::isfinite(result) && result > 0.0f) ? result
                                                 : 0.0f;
}
