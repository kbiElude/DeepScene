/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Tests for PushShiftPlanner: how far a draw that the recipe pushes is moved sideways for an eye.
 */
#include <gtest/gtest.h>
#include "analysis/recipe/recipe_applier.h"
#include "stereo/core/push_shift_planner.h"

namespace
{
    Analysis::Recipe::Recipe make_recipe(const float& in_overlay_parallax,
                                         const float& in_max_near_parallax)
    {
        Analysis::Recipe::Recipe result;

        result.comfort.max_near_parallax = in_max_near_parallax;
        result.comfort.overlay_parallax  = in_overlay_parallax;

        return result;
    }
}

TEST(PushShiftPlanner, TheLeftPictureGoesToTheRightAndTheRightOneToTheLeftByHalfTheParallaxEach)
{
    const Analysis::Recipe::RecipeApplier applier(make_recipe(0.01f,
                                                              0.0f) );

    EXPECT_FLOAT_EQ(Stereo::Core::PushShiftPlanner::plan(applier,
                                                         Analysis::Recipe::Eye::LEFT,
                                                         applier.get_overlay_parallax(),
                                                         false, /* in_is_affected */
                                                         640.0f,
                                                         8.0f,
                                                         0.0f),
                    3.2f);
    EXPECT_FLOAT_EQ(Stereo::Core::PushShiftPlanner::plan(applier,
                                                         Analysis::Recipe::Eye::RIGHT,
                                                         applier.get_overlay_parallax(),
                                                         false, /* in_is_affected */
                                                         640.0f,
                                                         8.0f,
                                                         0.0f),
                    -3.2f);
}

TEST(PushShiftPlanner, ARecipeWithNoOverlayParallaxOrAViewWithNoWidthMovesNothing)
{
    const Analysis::Recipe::RecipeApplier none(make_recipe(0.0f,
                                                           0.0f) );
    const Analysis::Recipe::RecipeApplier some(make_recipe(0.01f,
                                                           0.0f) );

    EXPECT_FLOAT_EQ(Stereo::Core::PushShiftPlanner::plan(none,
                                                         Analysis::Recipe::Eye::LEFT,
                                                         none.get_overlay_parallax(),
                                                         false, /* in_is_affected */
                                                         640.0f,
                                                         8.0f,
                                                         0.0f),
                    0.0f);
    EXPECT_FLOAT_EQ(Stereo::Core::PushShiftPlanner::plan(some,
                                                         Analysis::Recipe::Eye::LEFT,
                                                         some.get_overlay_parallax(),
                                                         false, /* in_is_affected */
                                                         0.0f,
                                                         8.0f,
                                                         0.0f),
                    0.0f);
}

TEST(PushShiftPlanner, TheParallaxIsNeverMoreThanTheNearestOneTheRecipeAllows)
{
    const Analysis::Recipe::RecipeApplier applier(make_recipe(0.05f,
                                                              0.02f) );

    EXPECT_FLOAT_EQ(Stereo::Core::PushShiftPlanner::plan(applier,
                                                         Analysis::Recipe::Eye::LEFT,
                                                         applier.get_overlay_parallax(),
                                                         false, /* in_is_affected */
                                                         1000.0f,
                                                         8.0f,
                                                         0.0f),
                    10.0f);
}

TEST(PushShiftPlanner, ADrawThatIsAffectedIsGivenWhatIsLeftOfTheNearestParallaxAfterTheOneOfItsLayer)
{
    const Analysis::Recipe::RecipeApplier applier(make_recipe(0.02f,
                                                              0.02f) );

    /* The layer is apart by 5 / 1000 of the width; 0.015 is left. */
    EXPECT_NEAR(Stereo::Core::PushShiftPlanner::plan(applier,
                                                     Analysis::Recipe::Eye::LEFT,
                                                     applier.get_overlay_parallax(),
                                                     true, /* in_is_affected */
                                                     1000.0f,
                                                     5.0f,
                                                     0.0f),
                7.5f,
                1.0e-4f);

    /* The eyes of the scene are held at 2, so the layer is apart by 2 / 1000: 0.018 is left. */
    EXPECT_NEAR(Stereo::Core::PushShiftPlanner::plan(applier,
                                                     Analysis::Recipe::Eye::LEFT,
                                                     applier.get_overlay_parallax(),
                                                     true, /* in_is_affected */
                                                     1000.0f,
                                                     5.0f,
                                                     2.0f),
                9.0f,
                1.0e-4f);
}

TEST(PushShiftPlanner, ADrawWhoseLayerHasMoreThanTheNearestParallaxIsNotMoved)
{
    const Analysis::Recipe::RecipeApplier applier(make_recipe(0.02f,
                                                              0.012f) );

    EXPECT_FLOAT_EQ(Stereo::Core::PushShiftPlanner::plan(applier,
                                                         Analysis::Recipe::Eye::LEFT,
                                                         applier.get_overlay_parallax(),
                                                         true, /* in_is_affected */
                                                         625.0f,
                                                         8.0f,
                                                         0.0f),
                    0.0f);
}

TEST(PushShiftPlanner, ADrawThatIsPushedIsMadeWithTheViewportMovedByTheShiftTakenTowardZero)
{
    const auto right = Stereo::Core::PushShiftPlanner::make_viewport(0,
                                                                     640,
                                                                     6.19f,
                                                                     false);
    const auto left  = Stereo::Core::PushShiftPlanner::make_viewport(0,
                                                                     640,
                                                                     -6.19f,
                                                                     false);
    const auto none  = Stereo::Core::PushShiftPlanner::make_viewport(10,
                                                                     640,
                                                                     0.0f,
                                                                     false);

    EXPECT_EQ(right.x,     6);
    EXPECT_EQ(right.width, 640);
    EXPECT_EQ(left.x,      -6);
    EXPECT_EQ(left.width,  640);
    EXPECT_EQ(none.x,      10);
    EXPECT_EQ(none.width,  640);
}

TEST(PushShiftPlanner, AnOverlayThatReachesBothSidesIsMadeAsMuchWiderAsTheShiftOnEachSideSoNoEdgeIsBare)
{
    /* Moved to the right by 6, the picture would leave the 6 pixels at the left edge bare: it starts at the edge and goes 12 pixels further instead, so that its middle is where it was moved to. */
    const auto right = Stereo::Core::PushShiftPlanner::make_viewport(0,
                                                                     640,
                                                                     6.19f,
                                                                     true);

    EXPECT_EQ(right.x,     0);
    EXPECT_EQ(right.width, 652);
    EXPECT_EQ(right.x + right.width / 2,
              6 + 640 / 2);

    /* Moved to the left, it ends at the right edge. */
    const auto left = Stereo::Core::PushShiftPlanner::make_viewport(0,
                                                                    640,
                                                                    -6.19f,
                                                                    true);

    EXPECT_EQ(left.x,     -12);
    EXPECT_EQ(left.x + left.width,
              640);
    EXPECT_EQ(left.x + left.width / 2,
              -6 + 640 / 2);

    /* Both edges stay covered, whichever way it is moved, and a shift of nothing changes nothing. */
    EXPECT_LE(right.x,
              0);
    EXPECT_GE(right.x + right.width,
              640);
    EXPECT_LE(left.x,
              0);
    EXPECT_GE(left.x + left.width,
              640);

    const auto none = Stereo::Core::PushShiftPlanner::make_viewport(0,
                                                                    640,
                                                                    0.0f,
                                                                    true);

    EXPECT_EQ(none.x,     0);
    EXPECT_EQ(none.width, 640);
}
