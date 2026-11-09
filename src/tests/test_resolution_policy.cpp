/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Tests for the resolution policy: what a converter makes the sizes of an application, and what it will not.
 */
#include <gtest/gtest.h>
#include "stereo/core/resolution_policy.h"
#include <math.h>

TEST(ResolutionPolicyTest, AScaleOfOneChangesNothing)
{
    const Stereo::Core::ResolutionPolicy policy(1.0f);

    EXPECT_TRUE(policy.is_identity() );
    EXPECT_EQ  (policy.scale_extent(800),
                800u);
    EXPECT_EQ  (policy.scale_extent(0),
                0u);
}

TEST(ResolutionPolicyTest, ExtentsAreMultipliedAndRoundedToTheNearestPixel)
{
    const Stereo::Core::ResolutionPolicy twice(2.0f);
    const Stereo::Core::ResolutionPolicy one_and_a_half(1.5f);

    EXPECT_FALSE(twice.is_identity() );
    EXPECT_EQ   (twice.scale_extent(800),
                 1600u);
    EXPECT_EQ   (twice.scale_extent(45),
                 90u);

    /* 67.5 is 68; 765 is exact. */
    EXPECT_EQ   (one_and_a_half.scale_extent(45),
                 68u);
    EXPECT_EQ   (one_and_a_half.scale_extent(510),
                 765u);
}

TEST(ResolutionPolicyTest, AScaleOutsideWhatAConverterCanDoIsBroughtInside)
{
    EXPECT_FLOAT_EQ(Stereo::Core::ResolutionPolicy(0.25f).get_scale(),
                    Stereo::Core::ResolutionPolicy::get_min_scale() );
    EXPECT_FLOAT_EQ(Stereo::Core::ResolutionPolicy(-3.0f).get_scale(),
                    Stereo::Core::ResolutionPolicy::get_min_scale() );
    EXPECT_FLOAT_EQ(Stereo::Core::ResolutionPolicy(100.0f).get_scale(),
                    Stereo::Core::ResolutionPolicy::get_max_scale() );
    EXPECT_FLOAT_EQ(Stereo::Core::ResolutionPolicy(NAN).get_scale(),
                    Stereo::Core::ResolutionPolicy::get_min_scale() );
    EXPECT_FLOAT_EQ(Stereo::Core::ResolutionPolicy(INFINITY).get_scale(),
                    Stereo::Core::ResolutionPolicy::get_min_scale() );
}

TEST(ResolutionPolicyTest, TheScaleThatFitsAnImageInATargetKeepsItsProportions)
{
    /* 800 x 600 in an eye of 2064 x 2208: the width is what limits. */
    EXPECT_FLOAT_EQ(Stereo::Core::ResolutionPolicy::get_fit_scale(2064, 2208, 800, 600),
                    2064.0f / 800.0f);

    /* A wide image in a tall target, and a tall one in a wide target. */
    EXPECT_FLOAT_EQ(Stereo::Core::ResolutionPolicy::get_fit_scale(1000, 3000, 800, 400),
                    1000.0f / 800.0f);
    EXPECT_FLOAT_EQ(Stereo::Core::ResolutionPolicy::get_fit_scale(3000, 1000, 400, 800),
                    1000.0f / 800.0f);

    /* A target that is smaller than the image gives less than one, which a policy brings up to what it can do. */
    EXPECT_LT(Stereo::Core::ResolutionPolicy::get_fit_scale(400, 300, 800, 600),
              1.0f);
    EXPECT_FLOAT_EQ(Stereo::Core::ResolutionPolicy(Stereo::Core::ResolutionPolicy::get_fit_scale(400, 300, 800, 600) ).get_scale(),
                    1.0f);

    /* Without a target, or an image, nothing is changed. */
    EXPECT_FLOAT_EQ(Stereo::Core::ResolutionPolicy::get_fit_scale(0, 2208, 800, 600),
                    1.0f);
    EXPECT_FLOAT_EQ(Stereo::Core::ResolutionPolicy::get_fit_scale(2064, 2208, 0, 600),
                    1.0f);
}

TEST(ResolutionPolicyTest, ARectangleHasEachOfItsFourNumbersMadeAsLargeAndRounded)
{
    const Stereo::Core::ResolutionPolicy policy(2.5f);
    Stereo::Core::PixelRect              rect;

    rect.height = 7;
    rect.width  = 10;
    rect.x      = 3;
    rect.y      = 1;

    const auto scaled = policy.scale_rect(rect);

    EXPECT_EQ(scaled.height,
              policy.scale_extent(7) );
    EXPECT_EQ(scaled.width,
              25u);
    EXPECT_EQ(scaled.x,
              policy.scale_extent(3) );
    EXPECT_EQ(scaled.y,
              policy.scale_extent(1) );
}

TEST(ResolutionPolicyTest, ARectangleWithBoundsIsKeptInsideThemWhereRoundingWouldTakeItOut)
{
    const Stereo::Core::ResolutionPolicy policy(2.5f);
    Stereo::Core::PixelRect              rect;

    /* 5 * 2.5 = 12.5 rounds up to 13, and 3 * 2.5 = 7.5 to 8: 21 of a target that is 20 wide. */
    rect.height = 4;
    rect.width  = 5;
    rect.x      = 3;
    rect.y      = 0;

    const auto scaled = policy.scale_rect(rect,
                                          20,
                                          10);

    EXPECT_EQ(scaled.x,
              8u);
    EXPECT_EQ(scaled.width,
              12u);
    EXPECT_EQ(scaled.height,
              10u);

    /* One that begins outside has nothing left. */
    rect.x = 9;

    EXPECT_EQ(policy.scale_rect(rect,
                                20,
                                10).width,
              0u);
}

TEST(ResolutionPolicyTest, AnIdentityPolicyLeavesARectangleAsItIs)
{
    const Stereo::Core::ResolutionPolicy policy(1.0f);
    Stereo::Core::PixelRect              rect;

    rect.height = 480;
    rect.width  = 640;
    rect.x      = 0;
    rect.y      = 50;

    const auto scaled = policy.scale_rect(rect,
                                          640,
                                          480);

    EXPECT_EQ(scaled.height,
              430u); /* kept inside the 480 the target has: 50 + 480 is more */
    EXPECT_EQ(scaled.width,
              640u);
    EXPECT_EQ(scaled.y,
              50u);
}
