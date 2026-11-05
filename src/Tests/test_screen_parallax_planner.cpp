/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Tests for ScreenParallaxPlanner: what is done to a draw of vertices in the space of the picture, whichever adapter asks.
 */
#include <gtest/gtest.h>
#include "Analysis/RecipeApplier/recipe_applier.h"
#include "Stereo/Core/screen_parallax_planner.h"

namespace
{
    constexpr uint64_t COVERING_KEY = 0xc0ffee;

    Analysis::RecipeLoader::Recipe make_recipe(const bool&  in_has_screen_parallax,
                                               const float& in_overlay_parallax)
    {
        Analysis::RecipeLoader::Recipe result;

        result.comfort.overlay_parallax            = in_overlay_parallax;
        result.covering_draw_key_vec.push_back(COVERING_KEY);
        result.screen_parallax.convergence_distance = 5.0f;
        result.screen_parallax.field_of_view_degrees = 90.0f;
        result.screen_parallax.is_enabled           = in_has_screen_parallax;

        return result;
    }

    Stereo::Core::ScreenParallaxPlanner::Input make_input(const bool& in_is_affected)
    {
        Stereo::Core::ScreenParallaxPlanner::Input result;

        result.eye_separation    = 2.0f;
        result.is_affected       = in_is_affected;
        result.is_overlay_pushed = true;
        result.is_presented      = true;
        result.picture_width     = 640.0f;

        return result;
    }
}

TEST(ScreenParallaxPlanner, AnAffectedDrawIsMovedByItsDepthAtTheDistanceOfTheSceneAndNeverFurtherApartThanItsLimit)
{
    const Analysis::RecipeApplier::RecipeApplier applier(make_recipe(true,
                                                                     0.0f) );
    auto                                         input = make_input(true);

    input.convergence_distance = 40.0f;
    input.separation_limit     = 0.5f;

    const auto plan = Stereo::Core::ScreenParallaxPlanner::plan(applier,
                                                                input);

    EXPECT_EQ(plan.kind,
              Stereo::Core::ScreenShiftPlan::Kind::PER_VERTEX);
    EXPECT_FLOAT_EQ(plan.convergence_distance,
                    40.0f);
    EXPECT_FLOAT_EQ(plan.eye_separation,
                    0.5f);
    EXPECT_FLOAT_EQ(plan.field_of_view_degrees,
                    90.0f);
    EXPECT_FLOAT_EQ(plan.picture_width,
                    640.0f);
}

TEST(ScreenParallaxPlanner, ASceneWithNoDistanceHasTheDistanceOfTheRecipeAndALimitAboveTheSeparationChangesNothing)
{
    const Analysis::RecipeApplier::RecipeApplier applier(make_recipe(true,
                                                                     0.0f) );
    auto                                         input = make_input(true);

    input.separation_limit = 9.0f;

    const auto plan = Stereo::Core::ScreenParallaxPlanner::plan(applier,
                                                                input);

    EXPECT_FLOAT_EQ(plan.convergence_distance,
                    5.0f);
    EXPECT_FLOAT_EQ(plan.eye_separation,
                    2.0f);
}

TEST(ScreenParallaxPlanner, ADrawWithoutScreenParallaxInTheRecipeIsLeftAsItIs)
{
    const Analysis::RecipeApplier::RecipeApplier applier(make_recipe(false,
                                                                     0.0f) );

    EXPECT_EQ(Stereo::Core::ScreenParallaxPlanner::plan(applier,
                                                        make_input(true) ).kind,
              Stereo::Core::ScreenShiftPlan::Kind::NONE);
}

TEST(ScreenParallaxPlanner, ADrawWhichCoversTheViewIsNeverMovedSideways)
{
    const Analysis::RecipeApplier::RecipeApplier applier(make_recipe(true,
                                                                     0.1f) );
    auto                                         affected   = make_input(true);
    auto                                         unaffected = make_input(false);

    affected.cover_key   = COVERING_KEY;
    unaffected.cover_key = COVERING_KEY;

    EXPECT_EQ(Stereo::Core::ScreenParallaxPlanner::plan(applier,
                                                        affected).kind,
              Stereo::Core::ScreenShiftPlan::Kind::NONE);
    EXPECT_EQ(Stereo::Core::ScreenParallaxPlanner::plan(applier,
                                                        unaffected).kind,
              Stereo::Core::ScreenShiftPlan::Kind::NONE);
}

TEST(ScreenParallaxPlanner, ADrawWithTheKeyOfACoveringOneThatDoesNotReachBothSidesIsMovedLikeAnyOther)
{
    const Analysis::RecipeApplier::RecipeApplier applier(make_recipe(true,
                                                                     0.1f) );
    auto                                         input = make_input(true);

    input.cover_key          = COVERING_KEY;
    input.has_extent         = true;
    input.reaches_both_sides = false;

    EXPECT_EQ(Stereo::Core::ScreenParallaxPlanner::plan(applier,
                                                        input).kind,
              Stereo::Core::ScreenShiftPlan::Kind::PER_VERTEX);

    input.reaches_both_sides = true;

    EXPECT_EQ(Stereo::Core::ScreenParallaxPlanner::plan(applier,
                                                        input).kind,
              Stereo::Core::ScreenShiftPlan::Kind::NONE);
}

TEST(ScreenParallaxPlanner, AnOverlayIsPushedHalfTheAmountToEachSideTheOtherWayForEachEye)
{
    const Analysis::RecipeApplier::RecipeApplier applier(make_recipe(true,
                                                                     0.05f) );
    auto                                         left  = make_input(false);
    auto                                         right = make_input(false);

    right.eye = Analysis::RecipeLoader::Eye::RIGHT;

    const auto left_plan  = Stereo::Core::ScreenParallaxPlanner::plan(applier,
                                                                      left);
    const auto right_plan = Stereo::Core::ScreenParallaxPlanner::plan(applier,
                                                                      right);

    EXPECT_EQ(left_plan.kind,
              Stereo::Core::ScreenShiftPlan::Kind::UNIFORM);
    EXPECT_FLOAT_EQ(left_plan.uniform_shift_pixels,
                    16.0f);
    EXPECT_FLOAT_EQ(right_plan.uniform_shift_pixels,
                    -16.0f);
}

TEST(ScreenParallaxPlanner, AnOverlayIsNotPushedWhenItIsNotPresentedNotWantedOrTheRecipeHasNoAmount)
{
    const Analysis::RecipeApplier::RecipeApplier pushing(make_recipe(true,
                                                                     0.05f) );
    const Analysis::RecipeApplier::RecipeApplier not_pushing(make_recipe(true,
                                                                         0.0f) );
    auto                                         not_presented = make_input(false);
    auto                                         not_wanted    = make_input(false);

    not_presented.is_presented    = false;
    not_wanted.is_overlay_pushed = false;

    EXPECT_EQ(Stereo::Core::ScreenParallaxPlanner::plan(pushing,
                                                        not_presented).kind,
              Stereo::Core::ScreenShiftPlan::Kind::NONE);
    EXPECT_EQ(Stereo::Core::ScreenParallaxPlanner::plan(pushing,
                                                        not_wanted).kind,
              Stereo::Core::ScreenShiftPlan::Kind::NONE);
    EXPECT_EQ(Stereo::Core::ScreenParallaxPlanner::plan(not_pushing,
                                                        make_input(false) ).kind,
              Stereo::Core::ScreenShiftPlan::Kind::NONE);
}
