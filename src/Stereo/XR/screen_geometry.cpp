/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "Stereo/XR/screen_geometry.h"
#include <algorithm>
#include <math.h>

/* What of the view the screen takes: the edge of the view is where the lenses are worst, and a screen up to it would be cut off by the
 * eyes which look a little away from the middle.
 */
constexpr float FILL_OF_VIEW = 0.92f;

/* A screen wider than half of the way around, or narrower than a few degrees, is not a screen of a headset. */
constexpr float MAX_CENTRAL_ANGLE = 3.0f;
constexpr float MIN_CENTRAL_ANGLE = 0.1f;


Stereo::XR::CylinderScreen Stereo::XR::ScreenGeometry::fit_cylinder(const FieldOfView& in_field_of_view,
                                                                    const float&       in_aspect_ratio,
                                                                    const float&       in_distance,
                                                                    const float&       in_scale)
{
    /* Along the curve, the screen is as wide as the angle it spans; its height is that over the aspect ratio, and is seen from the
     * middle of the cylinder as the angle whose tangent is half of it over the radius.
     */
    const float    available_horizontal = in_field_of_view.angle_right - in_field_of_view.angle_left;
    const float    available_vertical   = in_field_of_view.angle_up    - in_field_of_view.angle_down;
    CylinderScreen result;

    result.aspect_ratio  = in_aspect_ratio;
    result.central_angle = std::max(MIN_CENTRAL_ANGLE,
                                    std::min(MAX_CENTRAL_ANGLE,
                                             std::min(available_horizontal,
                                                      2.0f * in_aspect_ratio * ::tanf(available_vertical / 2.0f) ) * FILL_OF_VIEW * in_scale) );
    result.radius        = in_distance;

    return result;
}

Stereo::XR::QuadScreen Stereo::XR::ScreenGeometry::fit_quad(const FieldOfView& in_field_of_view,
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

Stereo::XR::FieldOfView Stereo::XR::ScreenGeometry::get_common_field_of_view(const FieldOfView& in_first,
                                                                             const FieldOfView& in_second)
{
    /* Both eyes see where they overlap; the screen is in the middle, so it reaches as far to one side as to the other. */
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

float Stereo::XR::ScreenGeometry::get_yaw(const float& in_x,
                                          const float& in_y,
                                          const float& in_z,
                                          const float& in_w)
{
    /* What the orientation does to a vector which looks down -Z, as the angle it is turned about the vertical axis. */
    return ::atan2f(2.0f        * (in_x * in_z + in_w * in_y),
                    1.0f - 2.0f * (in_x * in_x + in_y * in_y) );
}
