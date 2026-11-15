/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef HEADSET_XR_SCREEN_GEOMETRY_H
#define HEADSET_XR_SCREEN_GEOMETRY_H

namespace Headset
{
    namespace XR
    {
        /* How far an eye sees from the direction it looks in, in radians. Down and left are negative, as in OpenXR. */
        struct FieldOfView
        {
            float angle_down  = 0.0f;
            float angle_left  = 0.0f;
            float angle_right = 0.0f;
            float angle_up    = 0.0f;
        };

        /* A section of a cylinder around the viewer, seen from inside. It is a screen that curves around the viewer. */
        struct CylinderScreen
        {
            float aspect_ratio  = 1.0f; /* of the section: its width along the curve divided by its height         */
            float central_angle = 0.0f; /* the angle that the section spans around the cylinder, in radians        */
            float radius        = 1.0f;
        };

        /* A rectangle in front of the viewer. */
        struct QuadScreen
        {
            float height = 0.0f;
            float width  = 0.0f;
        };

        /* Decides where the screen of a headset is and how large it is: as large as the view allows, whichever headset it is. The head is
         * where the pose of the view says. The screen is in front of the head and does not follow it.
         */
        class ScreenGeometry
        {
        public:
            /* Both functions return the screen that fills as much of the field of view as an image with the given aspect ratio (width divided by height)
             * can. The screen fills either all of the available width or all of the available height, whichever limits it. in_scale scales
             * the result relative to what is available (1 fills it).
             */
            static CylinderScreen fit_cylinder(const FieldOfView& in_field_of_view,
                                               const float&       in_aspect_ratio,
                                               const float&       in_distance,
                                               const float&       in_scale);
            static QuadScreen     fit_quad    (const FieldOfView& in_field_of_view,
                                               const float&       in_aspect_ratio,
                                               const float&       in_distance,
                                               const float&       in_scale);

            /* Returns the part of the view that the screen can fill: where both eyes see, and as far to one side as to the other (the
             * screen is in the middle of the view).
             */
            static FieldOfView get_common_field_of_view(const FieldOfView& in_first,
                                                        const FieldOfView& in_second);

            /* Returns the direction that an orientation faces, as an angle about the vertical axis. 0 looks down the -Z axis, and the angle
             * grows as the direction turns toward -X. Pitch and roll are ignored.
             */
            static float get_yaw(const float& in_x,
                                 const float& in_y,
                                 const float& in_z,
                                 const float& in_w);
        };
    }
}

#endif /* HEADSET_XR_SCREEN_GEOMETRY_H */
