/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "stereo/core/screen_parallax_planner.h"

Stereo::Core::ScreenShiftPlan Stereo::Core::ScreenParallaxPlanner::plan(const Analysis::RecipeApplier::RecipeApplier& in_applier,
                                                                        const Input&                                  in_input)
{
    constexpr float HALF = 0.5f;

    const auto&     screen_parallax  = in_applier.get_screen_parallax();
    const float     overlay_parallax = in_applier.get_overlay_parallax();
    ScreenShiftPlan result;

    result.eye = in_input.eye;

    if (in_input.picture_width <= 0.0f                                                                               ||
        (in_applier.is_draw_covering(in_input.cover_key)                                                  &&
         (in_input.has_extent         == false                                                           ||
          in_input.reaches_both_sides == true) ) )
    {
        goto end;
    }

    if (in_input.is_affected)
    {
        if (screen_parallax.is_enabled)
        {
            result.convergence_distance  = (in_input.convergence_distance > 0.0f) ? in_input.convergence_distance
                                                                                  : screen_parallax.convergence_distance;
            result.eye_separation        = (in_input.separation_limit     > 0.0f  &&
                                            in_input.separation_limit     < in_input.eye_separation) ? in_input.separation_limit
                                                                                                     : in_input.eye_separation;
            result.field_of_view_degrees = screen_parallax.field_of_view_degrees;
            result.kind                  = ScreenShiftPlan::Kind::PER_VERTEX;
            result.picture_width         = in_input.picture_width;
        }
    }
    else
    if (in_input.is_overlay_pushed &&
        in_input.is_presented      &&
        overlay_parallax > 0.0f)
    {
        const float half_shift = overlay_parallax * in_input.picture_width * HALF;

        result.kind                 = ScreenShiftPlan::Kind::UNIFORM;
        result.uniform_shift_pixels = (in_input.eye == Analysis::RecipeLoader::Eye::LEFT) ? half_shift
                                                                                          : -half_shift;
    }

end:
    return result;
}
