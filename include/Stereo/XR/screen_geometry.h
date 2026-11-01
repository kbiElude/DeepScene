/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_XR_SCREEN_GEOMETRY_H
#define STEREO_XR_SCREEN_GEOMETRY_H

namespace Stereo
{
    namespace XR
    {
        /* How far an eye sees from the direction it looks in, in radians: down and left are negative, as OpenXR has them. */
        struct FieldOfView
        {
            float angle_down  = 0.0f;
            float angle_left  = 0.0f;
            float angle_right = 0.0f;
            float angle_up    = 0.0f;
        };

        /* A part of a cylinder around the viewer, which is seen from inside: the screen that curves with the viewer's view. */
        struct CylinderScreen
        {
            float aspect_ratio  = 1.0f; /* of the part of the surface: its width along the curve over its height */
            float central_angle = 0.0f; /* how much of the way around it spans, in radians                      */
            float radius        = 1.0f;
        };

        /* A rectangle in front of the viewer. */
        struct QuadScreen
        {
            float height = 0.0f;
            float width  = 0.0f;
        };

        /* Where the screen of a headset is, and how large: as large as the view lets it be, whichever headset it is. The head is where
         * the pose of the view says; the screen is in front of it, and does not follow it.
         */
        class ScreenGeometry
        {
        public:
            /* The screen which fills as much of the field of view as an image of the aspect ratio (its width over its height) can, which
             * is as much of the width of it as there is, or as much of the height. The scale is of what is left (1 fills it).
             */
            static CylinderScreen fit_cylinder(const FieldOfView& in_field_of_view,
                                               const float&       in_aspect_ratio,
                                               const float&       in_distance,
                                               const float&       in_scale);
            static QuadScreen     fit_quad    (const FieldOfView& in_field_of_view,
                                               const float&       in_aspect_ratio,
                                               const float&       in_distance,
                                               const float&       in_scale);

            /* The part of the view which the screen can fill: where both eyes see, and as far to each side as to the other (a screen is
             * in the middle of the view).
             */
            static FieldOfView get_common_field_of_view(const FieldOfView& in_first,
                                                        const FieldOfView& in_second);

            /* Which way an orientation looks, as an angle about the vertical axis, with 0 looking down the -Z axis and turning to -X as it
             * grows (the pitch and the roll are left out).
             */
            static float get_yaw(const float& in_x,
                                 const float& in_y,
                                 const float& in_z,
                                 const float& in_w);
        };
    }
}

#endif /* STEREO_XR_SCREEN_GEOMETRY_H */
