/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Tests for the resolution policy: what a converter makes the sizes of an application, and what it will not.
 */
#include <gtest/gtest.h>
#include "Stereo/StereoCore/resolution_policy.h"
#include <math.h>

TEST(ResolutionPolicyTest, AScaleOfOneChangesNothing)
{
    const Stereo::StereoCore::ResolutionPolicy policy(1.0f);

    EXPECT_TRUE(policy.is_identity() );
    EXPECT_EQ  (policy.scale_extent(800),
                800u);
    EXPECT_EQ  (policy.scale_extent(0),
                0u);
}

TEST(ResolutionPolicyTest, ExtentsAreMultipliedAndRoundedToTheNearestPixel)
{
    const Stereo::StereoCore::ResolutionPolicy twice(2.0f);
    const Stereo::StereoCore::ResolutionPolicy one_and_a_half(1.5f);

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
    EXPECT_FLOAT_EQ(Stereo::StereoCore::ResolutionPolicy(0.25f).get_scale(),
                    Stereo::StereoCore::ResolutionPolicy::get_min_scale() );
    EXPECT_FLOAT_EQ(Stereo::StereoCore::ResolutionPolicy(-3.0f).get_scale(),
                    Stereo::StereoCore::ResolutionPolicy::get_min_scale() );
    EXPECT_FLOAT_EQ(Stereo::StereoCore::ResolutionPolicy(100.0f).get_scale(),
                    Stereo::StereoCore::ResolutionPolicy::get_max_scale() );
    EXPECT_FLOAT_EQ(Stereo::StereoCore::ResolutionPolicy(NAN).get_scale(),
                    Stereo::StereoCore::ResolutionPolicy::get_min_scale() );
    EXPECT_FLOAT_EQ(Stereo::StereoCore::ResolutionPolicy(INFINITY).get_scale(),
                    Stereo::StereoCore::ResolutionPolicy::get_min_scale() );
}

TEST(ResolutionPolicyTest, TheScaleThatFitsAnImageInATargetKeepsItsProportions)
{
    /* 800 x 600 in an eye of 2064 x 2208: the width is what limits. */
    EXPECT_FLOAT_EQ(Stereo::StereoCore::ResolutionPolicy::get_fit_scale(2064, 2208, 800, 600),
                    2064.0f / 800.0f);

    /* A wide image in a tall target, and a tall one in a wide target. */
    EXPECT_FLOAT_EQ(Stereo::StereoCore::ResolutionPolicy::get_fit_scale(1000, 3000, 800, 400),
                    1000.0f / 800.0f);
    EXPECT_FLOAT_EQ(Stereo::StereoCore::ResolutionPolicy::get_fit_scale(3000, 1000, 400, 800),
                    1000.0f / 800.0f);

    /* A target that is smaller than the image gives less than one, which a policy brings up to what it can do. */
    EXPECT_LT(Stereo::StereoCore::ResolutionPolicy::get_fit_scale(400, 300, 800, 600),
              1.0f);
    EXPECT_FLOAT_EQ(Stereo::StereoCore::ResolutionPolicy(Stereo::StereoCore::ResolutionPolicy::get_fit_scale(400, 300, 800, 600) ).get_scale(),
                    1.0f);

    /* Without a target, or an image, nothing is changed. */
    EXPECT_FLOAT_EQ(Stereo::StereoCore::ResolutionPolicy::get_fit_scale(0, 2208, 800, 600),
                    1.0f);
    EXPECT_FLOAT_EQ(Stereo::StereoCore::ResolutionPolicy::get_fit_scale(2064, 2208, 0, 600),
                    1.0f);
}
