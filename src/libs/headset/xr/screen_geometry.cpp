/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "headset/xr/screen_geometry.h"
#include <algorithm>
#include <math.h>

/* The fraction of the field of view that the screen fills. The edge of the view is where the lenses are worst, and a screen that reached
 * the edge would be cut off by eyes that look slightly away from the middle.
 */
constexpr float FILL_OF_VIEW = 0.92f;

/* Limits for the angle that a cylinder screen spans, in radians: at most 3 (close to half the way around) and at least 0.1 (a few
 * degrees). A screen outside these limits is not a headset screen.
 */
constexpr float MAX_CENTRAL_ANGLE = 3.0f;
constexpr float MIN_CENTRAL_ANGLE = 0.1f;


Headset::XR::CylinderScreen Headset::XR::ScreenGeometry::fit_cylinder(const FieldOfView& in_field_of_view,
                                                                      const float&       in_aspect_ratio,
                                                                      const float&       in_distance,
                                                                      const float&       in_scale)
{
    /* At radius 1, the length of the screen along the curve equals the angle it spans. Its height is that width divided by the aspect
     * ratio. From the middle of the cylinder, the height spans the angle whose tangent is half the height over the radius. The vertical
     * limit is the largest width whose height fits in the available vertical angle.
     */
    const float    available_horizontal = in_field_of_view.angle_right - in_field_of_view.angle_left;
    const float    available_vertical   = in_field_of_view.angle_up    - in_field_of_view.angle_down;
    const float    vertical_limit       = 2.0f * in_aspect_ratio * ::tanf(available_vertical / 2.0f);
    const float    fitted_angle         = std::min(available_horizontal,
                                                   vertical_limit) * FILL_OF_VIEW * in_scale;
    const float    upper_limited_angle  = std::min(MAX_CENTRAL_ANGLE,
                                                   fitted_angle);
    CylinderScreen result;

    result.aspect_ratio  = in_aspect_ratio;
    result.central_angle = std::max(MIN_CENTRAL_ANGLE,
                                    upper_limited_angle);
    result.radius        = in_distance;

    return result;
}

Headset::XR::QuadScreen Headset::XR::ScreenGeometry::fit_quad(const FieldOfView& in_field_of_view,
                                                              const float&       in_aspect_ratio,
                                                              const float&       in_distance,
                                                              const float&       in_scale)
{
    const float available_horizontal = 2.0f * in_distance * ::tanf((in_field_of_view.angle_right - in_field_of_view.angle_left) / 2.0f);
    const float available_vertical   = 2.0f * in_distance * ::tanf((in_field_of_view.angle_up    - in_field_of_view.angle_down) / 2.0f);
    QuadScreen  result;

    result.width  = std::min(available_horizontal,
                             available_vertical * in_aspect_ratio) * FILL_OF_VIEW * in_scale;
    result.height = result.width / in_aspect_ratio;

    return result;
}

Headset::XR::FieldOfView Headset::XR::ScreenGeometry::get_common_field_of_view(const FieldOfView& in_first,
                                                                               const FieldOfView& in_second)
{
    /* Both eyes see the overlap. The screen is in the middle, so it reaches as far to one side as to the other. */
    const float down  = std::max(in_first.angle_down,
                                 in_second.angle_down);
    const float left  = std::max(in_first.angle_left,
                                 in_second.angle_left);
    const float right = std::min(in_first.angle_right,
                                 in_second.angle_right);
    const float up    = std::min(in_first.angle_up,
                                 in_second.angle_up);

    const float half_horizontal = std::min(-left,
                                            right);
    const float half_vertical   = std::min(-down,
                                            up);

    FieldOfView result;

    result.angle_down  = -half_vertical;
    result.angle_left  = -half_horizontal;
    result.angle_right = half_horizontal;
    result.angle_up    = half_vertical;

    return result;
}

float Headset::XR::ScreenGeometry::get_yaw(const float& in_x,
                                           const float& in_y,
                                           const float& in_z,
                                           const float& in_w)
{
    /* The angle about the vertical axis by which the orientation turns a vector that looks down -Z. */
    return ::atan2f(2.0f        * (in_x * in_z + in_w * in_y),
                    1.0f - 2.0f * (in_x * in_x + in_y * in_y) );
}
