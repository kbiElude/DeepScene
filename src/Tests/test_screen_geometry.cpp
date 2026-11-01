/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Checks of where the screen of a headset is put and how large it is: what of the view it fills.
 */
#include <gtest/gtest.h>
#include "Stereo/StereoXR/screen_geometry.h"
#include <math.h>

static Stereo::StereoXR::FieldOfView make_field_of_view(const float& in_down,
                                                        const float& in_left,
                                                        const float& in_right,
                                                        const float& in_up)
{
    Stereo::StereoXR::FieldOfView result;

    result.angle_down  = in_down;
    result.angle_left  = in_left;
    result.angle_right = in_right;
    result.angle_up    = in_up;

    return result;
}

TEST(ScreenGeometryTest, WhatBothEyesSeeIsTheViewAndItIsTheSameToEachSide)
{
    /* The left eye sees farther to the left, the right eye farther to the right: where they meet is the narrower. */
    const auto common = Stereo::StereoXR::ScreenGeometry::get_common_field_of_view(make_field_of_view(-0.7f, -0.9f, 0.6f, 0.8f),
                                                                                   make_field_of_view(-0.8f, -0.6f, 0.9f, 0.7f) );

    EXPECT_FLOAT_EQ(common.angle_left,
                    -0.6f);
    EXPECT_FLOAT_EQ(common.angle_right,
                    0.6f);
    EXPECT_FLOAT_EQ(common.angle_down,
                    -0.7f);
    EXPECT_FLOAT_EQ(common.angle_up,
                    0.7f);
}

TEST(ScreenGeometryTest, TheYawIsWhichWayTheOrientationLooksAboutTheVerticalAxisWhateverItsPitch)
{
    constexpr float QUARTER_TURN = 1.57079633f; /* half of pi */

    EXPECT_NEAR(Stereo::StereoXR::ScreenGeometry::get_yaw(0.0f, 0.0f, 0.0f, 1.0f),
                0.0f,
                1.0e-6f);

    /* A quarter turn about the vertical axis. */
    EXPECT_NEAR(Stereo::StereoXR::ScreenGeometry::get_yaw(0.0f, ::sinf(QUARTER_TURN / 2.0f), 0.0f, ::cosf(QUARTER_TURN / 2.0f) ),
                QUARTER_TURN,
                1.0e-5f);

    /* The same, looking down by a part of a turn about the X axis first: a quaternion of the yaw times that of the pitch. */
    {
        const float pitch_half = 0.3f / 2.0f;
        const float yaw_half   = QUARTER_TURN / 2.0f;
        const float yaw_y      = ::sinf(yaw_half);
        const float yaw_w      = ::cosf(yaw_half);
        const float pitch_x    = ::sinf(pitch_half);
        const float pitch_w    = ::cosf(pitch_half);

        /* The product of the quaternion of the yaw (0, sin, 0, cos) and that of the pitch (sin, 0, 0, cos). */
        EXPECT_NEAR(Stereo::StereoXR::ScreenGeometry::get_yaw(yaw_w * pitch_x,
                                                              yaw_y * pitch_w,
                                                              -yaw_y * pitch_x,
                                                              yaw_w * pitch_w),
                    QUARTER_TURN,
                    1.0e-4f);
    }
}

TEST(ScreenGeometryTest, TheCylinderFillsTheWidthOfAViewWhichIsWiderThanTheImageAndItsHeightWhenItIsTaller)
{
    /* A wide view and a 4:3 image: the width of the view is the limit. */
    const auto wide   = Stereo::StereoXR::ScreenGeometry::fit_cylinder(make_field_of_view(-0.7f, -0.9f, 0.9f, 0.7f),
                                                                       4.0f / 3.0f,
                                                                       2.0f,
                                                                       1.0f);

    EXPECT_NEAR(wide.central_angle,
                1.8f * 0.92f,
                1.0e-4f);
    EXPECT_FLOAT_EQ(wide.aspect_ratio,
                    4.0f / 3.0f);
    EXPECT_FLOAT_EQ(wide.radius,
                    2.0f);

    /* A tall view and a wide image: the height of the view is. The height seen is 2 * atan(angle / (2 * aspect)): a tangent back. */
    const auto tall = Stereo::StereoXR::ScreenGeometry::fit_cylinder(make_field_of_view(-0.3f, -1.2f, 1.2f, 0.3f),
                                                                     16.0f / 9.0f,
                                                                     2.0f,
                                                                     1.0f);

    EXPECT_NEAR(tall.central_angle,
                2.0f * (16.0f / 9.0f) * ::tanf(0.3f) * 0.92f,
                1.0e-4f);
}

TEST(ScreenGeometryTest, TheScaleMakesTheScreenSmallerAndWhatIsNotAScreenIsKeptWithinLimits)
{
    const auto field_of_view = make_field_of_view(-0.7f, -0.9f, 0.9f, 0.7f);
    const auto full          = Stereo::StereoXR::ScreenGeometry::fit_cylinder(field_of_view, 4.0f / 3.0f, 2.0f, 1.0f);
    const auto half          = Stereo::StereoXR::ScreenGeometry::fit_cylinder(field_of_view, 4.0f / 3.0f, 2.0f, 0.5f);

    EXPECT_NEAR(half.central_angle,
                full.central_angle / 2.0f,
                1.0e-5f);

    /* A view of nothing is not a screen of nothing. */
    EXPECT_GT(Stereo::StereoXR::ScreenGeometry::fit_cylinder(make_field_of_view(0.0f, 0.0f, 0.0f, 0.0f), 1.0f, 2.0f, 1.0f).central_angle,
              0.0f);
    EXPECT_LE(Stereo::StereoXR::ScreenGeometry::fit_cylinder(make_field_of_view(-1.5f, -3.0f, 3.0f, 1.5f), 4.0f, 2.0f, 1.0f).central_angle,
              3.0f);
}

TEST(ScreenGeometryTest, TheQuadHasTheAspectRatioOfTheImageAndFitsTheViewAtItsDistance)
{
    const auto quad = Stereo::StereoXR::ScreenGeometry::fit_quad(make_field_of_view(-0.7f, -0.9f, 0.9f, 0.7f),
                                                                 4.0f / 3.0f,
                                                                 2.0f,
                                                                 1.0f);

    EXPECT_NEAR(quad.width / quad.height,
                4.0f / 3.0f,
                1.0e-5f);

    /* It is seen as wide as the view, or as tall as it, and no more. */
    EXPECT_LE(quad.width,
              2.0f * 2.0f * ::tanf(0.9f) );
    EXPECT_LE(quad.height,
              2.0f * 2.0f * ::tanf(0.7f) );
}
