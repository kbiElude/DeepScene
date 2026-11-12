/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "stereo/core/push_shift_planner.h"
#include <algorithm>

float Stereo::Core::PushShiftPlanner::plan(const Analysis::Recipe::RecipeApplier& in_applier,
                                           const Analysis::Recipe::Eye&           in_eye,
                                           const float&                           in_parallax,
                                           const bool&                            in_is_affected,
                                           const float&                           in_picture_width_pixels,
                                           const float&                           in_eye_separation,
                                           const float&                           in_separation_limit)
{
    constexpr float HALF = 0.5f;

    const auto& comfort  = in_applier.get_comfort();
    float       parallax = in_parallax;
    float       result   = 0.0f;

    if (parallax                <= 0.0f ||
        in_picture_width_pixels <= 0.0f)
    {
        goto end;
    }

    if (comfort.max_near_parallax > 0.0f)
    {
        float layer_parallax = 0.0f;

        if (in_is_affected)
        {
            const float separation = (in_separation_limit > 0.0f             &&
                                      in_eye_separation   > in_separation_limit) ? in_separation_limit
                                                                                 : in_eye_separation;

            layer_parallax = separation / in_picture_width_pixels;
        }

        parallax = std::min(parallax,
                            comfort.max_near_parallax - layer_parallax);
    }

    if (parallax <= 0.0f)
    {
        goto end;
    }

    /* Each eye gets half of it: the left picture goes to the right, and the right picture to the left. */
    result = (in_eye == Analysis::Recipe::Eye::LEFT) ?  parallax * in_picture_width_pixels * HALF
                                                     : -parallax * in_picture_width_pixels * HALF;

end:
    return result;
}
