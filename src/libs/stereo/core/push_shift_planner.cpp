/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "stereo/core/push_shift_planner.h"
#include <algorithm>

float Stereo::Core::PushShiftPlanner::get_screen_layer_parallax(const bool&  in_is_affected,
                                                                const float& in_picture_width_pixels,
                                                                const float& in_eye_separation,
                                                                const float& in_separation_limit)
{
    float result = 0.0f;

    if (in_is_affected &&
        in_picture_width_pixels > 0.0f)
    {
        const float separation = (in_separation_limit > 0.0f             &&
                                  in_eye_separation   > in_separation_limit) ? in_separation_limit
                                                                             : in_eye_separation;

        result = separation / in_picture_width_pixels;
    }

    return result;
}

Stereo::Core::PushShiftPlanner::PushedViewport Stereo::Core::PushShiftPlanner::make_viewport(const int32_t& in_x,
                                                                                            const int32_t& in_width,
                                                                                            const float&   in_shift_pixels,
                                                                                            const bool&    in_is_side_reaching)
{
    /* Toward zero, so that the shift never goes past what the planner allowed. */
    const int32_t shift_pixels = static_cast<int32_t>(in_shift_pixels);
    const int32_t widening     = (in_is_side_reaching) ? ((shift_pixels < 0) ? -shift_pixels : shift_pixels)
                                                       : 0;
    PushedViewport result;

    result.width = in_width + 2 * widening;
    result.x     = in_x + shift_pixels - widening;

    return result;
}

float Stereo::Core::PushShiftPlanner::plan(const Analysis::Recipe::RecipeApplier& in_applier,
                                           const Analysis::Recipe::Eye&           in_eye,
                                           const float&                           in_parallax,
                                           const bool&                            in_is_affected,
                                           const float&                           in_picture_width_pixels,
                                           const float&                           in_eye_separation,
                                           const float&                           in_separation_limit)
{
    return plan_with_layer(in_applier,
                           in_eye,
                           in_parallax,
                           get_screen_layer_parallax(in_is_affected,
                                                     in_picture_width_pixels,
                                                     in_eye_separation,
                                                     in_separation_limit),
                           in_picture_width_pixels);
}

float Stereo::Core::PushShiftPlanner::plan_with_layer(const Analysis::Recipe::RecipeApplier& in_applier,
                                                      const Analysis::Recipe::Eye&           in_eye,
                                                      const float&                           in_parallax,
                                                      const float&                           in_layer_parallax,
                                                      const float&                           in_picture_width_pixels)
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
        parallax = std::min(parallax,
                            comfort.max_near_parallax - in_layer_parallax);
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
