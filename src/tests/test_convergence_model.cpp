/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Tests for ConvergenceModel: the distance the eyes converge at, and the limit of how far apart they may be.
 */
#include <gtest/gtest.h>
#include "stereo/core/convergence_model.h"
#include <math.h>

TEST(ConvergenceModel, TheSeparationIsAThirtiethOfTheDistanceTimesTheStrengthWhenThereIsNoLimit)
{
    Stereo::Core::ConvergenceModel model;

    model.set_convergence_distance(60.0f);
    model.set_depth_strength      (1.5f);

    EXPECT_FLOAT_EQ(model.get_effective_eye_separation(),
                    3.0f);
}

TEST(ConvergenceModel, TheLimitOfTheSceneIsACeilingWhichNoStrengthLifts)
{
    Stereo::Core::ConvergenceModel model;

    model.set_convergence_distance(60.0f);
    model.set_depth_strength      (2.0f);
    model.follow_scene_max_separation(0.5f);

    /* More strength than the natural one is not more than the scene allows. */
    EXPECT_FLOAT_EQ(model.get_effective_eye_separation(),
                    0.5f);

    /* Less strength brings the limit down with the separation. */
    model.set_depth_strength(0.5f);

    EXPECT_FLOAT_EQ(model.get_effective_eye_separation(),
                    0.25f);

    /* Where the limit is far above the natural separation, more strength is more depth. */
    model.set_depth_strength(1.25f);
    model.follow_scene_max_separation(0.0f);
    model.follow_scene_max_separation(100.0f);

    EXPECT_NEAR(model.get_effective_eye_separation(),
                2.5f,
                0.01f);

    model.set_depth_strength(2.0f);

    /* A limit above what the distance gives does nothing. */
    model.follow_scene_max_separation(0.0f);
    model.follow_scene_max_separation(100.0f);

    EXPECT_FLOAT_EQ(model.get_effective_eye_separation(),
                    4.0f);
}

TEST(ConvergenceModel, ASceneWithNoLimitLiftsItAtOnce)
{
    Stereo::Core::ConvergenceModel model;

    model.set_convergence_distance(30.0f);
    model.set_depth_strength      (1.0f);
    model.follow_scene_max_separation(0.1f);

    EXPECT_FLOAT_EQ(model.get_effective_eye_separation(),
                    0.1f);

    model.follow_scene_max_separation(0.0f);

    EXPECT_FLOAT_EQ(model.get_effective_eye_separation(),
                    1.0f);
}

TEST(ConvergenceModel, TheLimitIsTakenAtOnceAtFirstAndThenEasedTowardTheNextOneAlongTheLogarithm)
{
    Stereo::Core::ConvergenceModel model;

    model.set_convergence_distance(300.0f); /* 10 without a limit */
    model.set_depth_strength      (1.0f);
    model.follow_scene_max_separation(1.0f);

    EXPECT_FLOAT_EQ(model.get_effective_eye_separation(),
                    1.0f);

    model.follow_scene_max_separation(4.0f);

    const float after_one_frame = model.get_effective_eye_separation();

    EXPECT_GT(after_one_frame,
              1.0f);
    EXPECT_LT(after_one_frame,
              2.0f);

    for (int n_frame = 0;
             n_frame < 400;
           ++n_frame)
    {
        model.follow_scene_max_separation(4.0f);
    }

    EXPECT_FLOAT_EQ(model.get_effective_eye_separation(),
                    4.0f);
}

TEST(ConvergenceModel, TheLimitDoesNotApplyToASeparationThatWasSet)
{
    Stereo::Core::ConvergenceModel model;

    model.set_eye_separation(0.8f);
    model.follow_scene_max_separation(0.1f);

    EXPECT_FLOAT_EQ(model.get_effective_eye_separation(),
                    0.8f);
}

TEST(ConvergenceModel, TheEyesOfThePersonScaleTheNaturalSeparationButNotTheLimitOfTheScene)
{
    Stereo::Core::ConvergenceModel model;

    model.set_convergence_distance(30.0f);
    model.set_depth_strength      (1.0f);

    EXPECT_FLOAT_EQ(model.get_ipd_scale(),
                    1.0f);
    EXPECT_FLOAT_EQ(model.get_effective_eye_separation(),
                    1.0f);

    EXPECT_TRUE(model.set_ipd_scale(0.9f) );

    EXPECT_FLOAT_EQ(model.get_effective_eye_separation(),
                    0.9f);

    /* The limit of the scene is a limit of what the eyes bear at the nearest surface, whoever looks. */
    model.follow_scene_max_separation(0.5f);
    EXPECT_TRUE(model.set_ipd_scale(1.3f) );

    EXPECT_FLOAT_EQ(model.get_effective_eye_separation(),
                    0.5f);

    /* What is not a scale is refused, and the one there was stays. */
    EXPECT_FALSE(model.set_ipd_scale(0.0f) );
    EXPECT_FALSE(model.set_ipd_scale(-1.0f) );
    EXPECT_FALSE(model.set_ipd_scale(NAN) );
    EXPECT_FLOAT_EQ(model.get_ipd_scale(),
                    1.3f);
}
