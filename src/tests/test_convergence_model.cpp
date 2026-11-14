/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Tests for ConvergenceModel: the distance the eyes converge at, and the limit of how far apart they may be.
 */
#include <gtest/gtest.h>
#include "stereo/core/convergence_model.h"
#include <math.h>

namespace
{
    /* What a frame of 72 Hz goes toward its target. */
    constexpr float FRACTION_PER_FRAME = 0.05f;
}

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
    model.follow_scene_max_separation(0.5f, FRACTION_PER_FRAME);

    /* More strength than the natural one is not more than the scene allows. */
    EXPECT_FLOAT_EQ(model.get_effective_eye_separation(),
                    0.5f);

    /* Less strength brings the limit down with the separation. */
    model.set_depth_strength(0.5f);

    EXPECT_FLOAT_EQ(model.get_effective_eye_separation(),
                    0.25f);

    /* Where the limit is far above the natural separation, more strength is more depth. */
    model.set_depth_strength(1.25f);
    model.follow_scene_max_separation(0.0f, FRACTION_PER_FRAME);
    model.follow_scene_max_separation(100.0f, FRACTION_PER_FRAME);

    EXPECT_NEAR(model.get_effective_eye_separation(),
                2.5f,
                0.01f);

    model.set_depth_strength(2.0f);

    /* A limit above what the distance gives does nothing. */
    model.follow_scene_max_separation(0.0f, FRACTION_PER_FRAME);
    model.follow_scene_max_separation(100.0f, FRACTION_PER_FRAME);

    EXPECT_FLOAT_EQ(model.get_effective_eye_separation(),
                    4.0f);
}

TEST(ConvergenceModel, ASceneWithNoLimitLiftsItAtOnce)
{
    Stereo::Core::ConvergenceModel model;

    model.set_convergence_distance(30.0f);
    model.set_depth_strength      (1.0f);
    model.follow_scene_max_separation(0.1f, FRACTION_PER_FRAME);

    EXPECT_FLOAT_EQ(model.get_effective_eye_separation(),
                    0.1f);

    model.follow_scene_max_separation(0.0f, FRACTION_PER_FRAME);

    EXPECT_FLOAT_EQ(model.get_effective_eye_separation(),
                    1.0f);
}

TEST(ConvergenceModel, TheLimitIsTakenAtOnceAtFirstAndThenEasedTowardTheNextOneAlongTheLogarithm)
{
    Stereo::Core::ConvergenceModel model;

    model.set_convergence_distance(300.0f); /* 10 without a limit */
    model.set_depth_strength      (1.0f);
    model.follow_scene_max_separation(1.0f, FRACTION_PER_FRAME);

    EXPECT_FLOAT_EQ(model.get_effective_eye_separation(),
                    1.0f);

    model.follow_scene_max_separation(1.8f, FRACTION_PER_FRAME);

    const float after_one_frame = model.get_effective_eye_separation();

    EXPECT_GT(after_one_frame,
              1.0f);
    EXPECT_LT(after_one_frame,
              2.0f);

    for (int n_frame = 0;
             n_frame < 400;
           ++n_frame)
    {
        model.follow_scene_max_separation(1.8f, FRACTION_PER_FRAME);
    }

    EXPECT_FLOAT_EQ(model.get_effective_eye_separation(),
                    1.8f);
}

TEST(ConvergenceModel, ATargetMoreThanTwiceOrLessThanHalfTheOneBeforeIsACutAndIsTakenAtOnce)
{
    Stereo::Core::ConvergenceModel model;

    model.set_depth_strength(1.0f);
    model.follow_scene_convergence_distance(300.0f, FRACTION_PER_FRAME);
    model.follow_scene_max_separation      (1.0f,   FRACTION_PER_FRAME);

    /* A step that is not a cut is eased: the target is a long way off after a frame. */
    model.follow_scene_convergence_distance(500.0f, FRACTION_PER_FRAME);
    model.follow_scene_max_separation      (1.8f,   FRACTION_PER_FRAME);

    EXPECT_LT(model.get_effective_convergence_distance(),
              400.0f);
    EXPECT_LT(model.get_effective_eye_separation(),
              1.5f);

    /* A target of more than twice the one before is taken at once, and so is one of less than half of it. */
    model.follow_scene_convergence_distance(1500.0f, FRACTION_PER_FRAME);
    model.follow_scene_max_separation      (6.0f,    FRACTION_PER_FRAME);

    EXPECT_FLOAT_EQ(model.get_effective_convergence_distance(),
                    1500.0f);
    EXPECT_FLOAT_EQ(model.get_effective_eye_separation(),
                    6.0f);

    model.follow_scene_convergence_distance(300.0f, FRACTION_PER_FRAME);
    model.follow_scene_max_separation      (0.5f,   FRACTION_PER_FRAME);

    EXPECT_FLOAT_EQ(model.get_effective_convergence_distance(),
                    300.0f);
    EXPECT_FLOAT_EQ(model.get_effective_eye_separation(),
                    0.5f);
}

TEST(ConvergenceModel, ACutIsMeasuredAgainstTheTargetOfTheFrameBeforeAndNotAgainstWhereTheEyesAre)
{
    Stereo::Core::ConvergenceModel model;

    model.set_convergence_distance(30000.0f); /* the natural separation is far above the limit */
    model.set_depth_strength      (1.0f);
    model.follow_scene_max_separation(1.0f, FRACTION_PER_FRAME);

    /* A target that creeps up by a step that is no cut each frame is never a cut, however far the eyes lag behind it. */
    float target = 1.0f;

    for (int n_frame = 0;
             n_frame < 30;
           ++n_frame)
    {
        target *= 1.5f;

        model.follow_scene_max_separation(target, FRACTION_PER_FRAME);
    }

    EXPECT_LT(model.get_effective_eye_separation(),
              target / 10.0f);
}

TEST(ConvergenceModel, TheLimitDoesNotApplyToASeparationThatWasSet)
{
    Stereo::Core::ConvergenceModel model;

    model.set_eye_separation(0.8f);
    model.follow_scene_max_separation(0.1f, FRACTION_PER_FRAME);

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
    model.follow_scene_max_separation(0.5f, FRACTION_PER_FRAME);
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

TEST(ConvergenceModel, NoTimeGoesNoWayAndTheTimeOfAFrameOf72HertzGoesATwentiethOfIt)
{
    EXPECT_FLOAT_EQ(Stereo::Core::ConvergenceModel::get_fraction_of_the_way(0.0f),
                    0.0f);
    EXPECT_FLOAT_EQ(Stereo::Core::ConvergenceModel::get_fraction_of_the_way(-1.0f),
                    0.0f);
    EXPECT_NEAR   (Stereo::Core::ConvergenceModel::get_fraction_of_the_way(Stereo::Core::ConvergenceModel::get_reference_frame_seconds() ),
                   0.05f,
                   1e-6f);
}

TEST(ConvergenceModel, TheSameTimeGoesTheSameWayWhateverTheNumberOfFramesItIsMadeOf)
{
    const float reference_seconds = Stereo::Core::ConvergenceModel::get_reference_frame_seconds();

    /* Ten frames of a tenth of the time are as far as one: what is left after each is multiplied. */
    const float one_frame = Stereo::Core::ConvergenceModel::get_fraction_of_the_way(reference_seconds);
    const float tenth     = Stereo::Core::ConvergenceModel::get_fraction_of_the_way(reference_seconds / 10.0f);

    EXPECT_NEAR(1.0f - ::powf(1.0f - tenth,
                              10.0f),
                one_frame,
                1e-5f);

    /* The eyes that follow a scene for a second of frames of 72 Hz and for a second of 1,000 are in the same place. */
    Stereo::Core::ConvergenceModel slow_model;
    Stereo::Core::ConvergenceModel fast_model;

    slow_model.follow_scene_max_separation(1.0f,
                                           1.0f);
    fast_model.follow_scene_max_separation(1.0f,
                                           1.0f);

    const float frame_72    = Stereo::Core::ConvergenceModel::get_fraction_of_the_way(1.0f / 72.0f);
    const float frame_1000  = Stereo::Core::ConvergenceModel::get_fraction_of_the_way(1.0f / 1000.0f);

    for (int n_frame = 0;
             n_frame < 72;
           ++n_frame)
    {
        slow_model.follow_scene_max_separation(4.0f,
                                               frame_72);
    }

    for (int n_frame = 0;
             n_frame < 1000;
           ++n_frame)
    {
        fast_model.follow_scene_max_separation(4.0f,
                                               frame_1000);
    }

    slow_model.set_convergence_distance(60.0f);
    fast_model.set_convergence_distance(60.0f);
    slow_model.set_depth_strength      (10.0f);
    fast_model.set_depth_strength      (10.0f);

    EXPECT_NEAR(slow_model.get_effective_eye_separation(),
                fast_model.get_effective_eye_separation(),
                0.02f);
}

TEST(ConvergenceModel, AStallOfSecondsPutsTheEyesWhereTheSceneSaysAndTheFirstTargetIsTakenAtOnce)
{
    EXPECT_GT(Stereo::Core::ConvergenceModel::get_fraction_of_the_way(5.0f),
              0.999f);
    EXPECT_LE(Stereo::Core::ConvergenceModel::get_fraction_of_the_way(5.0f),
              1.0f);

    Stereo::Core::ConvergenceModel model;

    model.set_convergence_distance(60.0f);
    model.set_depth_strength      (10.0f);

    /* Nothing is there yet, so a fraction of no way still takes the target. */
    model.follow_scene_max_separation(2.0f,
                                      0.0f);

    EXPECT_FLOAT_EQ(model.get_effective_eye_separation(),
                    2.0f);
}
