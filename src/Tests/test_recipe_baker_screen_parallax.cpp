/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Tests for the recipe baker with vertices which no matrix moves (pre-transformed): the distance at which the eyes converge in a scene, and how far
 * apart they may be, are worked out from how deep the surfaces are, with the angle of view that the recipe says in the place of the projection.
 */
#include <gtest/gtest.h>
#include "Analysis/RecipeBake/recipe_baker.h"
#include "Analysis/RecipeLoader/recipe_loader.h"
#include "model_test_helper.h"

namespace
{
    /* A frame of a draw with vertices at a depth of 2 and one of 10, in a viewport of 640 x 480. */
    Analysis::APIDumpAnalysis::AnalysisResult make_analysis()
    {
        Analysis::APIDumpAnalysis::AnalysisResult analysis;
        Tests::ModelTestHelper                    helper;
        const auto                                swapchain_image = helper.create_swapchain_image();

        helper.bind        (swapchain_image);
        helper.set_viewport(0, 0, 640, 480);
        helper.draw_with_screen_geometry({{100.0f, 100.0f, 0.5f},
                                          {200.0f, 200.0f, 0.1f}},
                                         30);
        helper.end_frame();

        analysis.model = helper.get_model();

        return analysis;
    }
}

TEST(RecipeBakerScreenParallax, TheDistanceOfASceneAndHowFarApartTheEyesMayBeAreWorkedOutWithTheAngleOfViewOfTheRecipe)
{
    const auto                       analysis = make_analysis();
    Analysis::RecipeBake::BakeReport report;
    Analysis::RecipeLoader::Recipe   recipe;
    std::string                      error;
    std::string                      text;

    /* An angle of view of 90 degrees has a projection scale of 1. The nearest surface is 2 away, the scene converges at the middle, which is 10: the
     * eyes may be 2 * 0.03 / (1 / 2 - 1 / 10) = 0.15 apart.
     */
    ASSERT_TRUE(Analysis::RecipeBake::RecipeBaker::bake("{\"recipe_version\": 0, \"key_scheme_version\": 1, \"target\": {\"name\": \"test\", \"description\": \"a test\"},"
                                                        "\"comfort\": {\"max_near_parallax\": 0.03, \"near_fraction\": 0.5},"
                                                        "\"screen_parallax\": {\"convergence_distance\": 5, \"field_of_view_degrees\": 90},"
                                                        "\"rules\": []}",
                                                        analysis,
                                                       &text,
                                                       &report,
                                                       &error) ) << error;
    ASSERT_TRUE(Analysis::RecipeLoader::RecipeLoader::parse(text,
                                                           &recipe,
                                                           &error) ) << error;

    /* The recipe is kept as it was written. */
    EXPECT_TRUE(recipe.screen_parallax.is_enabled);

    ASSERT_EQ(recipe.scene_convergence_vec.size(),
              1u);
    EXPECT_NEAR(recipe.scene_convergence_vec.at(0).distance,
                10.0f,
                1.5f);

    ASSERT_EQ(recipe.scene_separation_vec.size(),
              1u);
    EXPECT_NEAR(recipe.scene_separation_vec.at(0).max_separation,
                0.15f,
                0.04f);

    /* A narrower angle of view makes the same separation more of the width, so the limit is smaller: the scale is the reciprocal of the tangent. */
    Analysis::RecipeLoader::Recipe narrow_recipe;

    ASSERT_TRUE(Analysis::RecipeBake::RecipeBaker::bake("{\"recipe_version\": 0, \"key_scheme_version\": 1, \"target\": {\"name\": \"test\", \"description\": \"a test\"},"
                                                        "\"comfort\": {\"max_near_parallax\": 0.03, \"near_fraction\": 0.5},"
                                                        "\"screen_parallax\": {\"convergence_distance\": 5, \"field_of_view_degrees\": 53.13},"
                                                        "\"rules\": []}",
                                                        analysis,
                                                       &text,
                                                       &report,
                                                       &error) ) << error;
    ASSERT_TRUE(Analysis::RecipeLoader::RecipeLoader::parse(text,
                                                           &narrow_recipe,
                                                           &error) ) << error;

    ASSERT_EQ(narrow_recipe.scene_separation_vec.size(),
              1u);
    EXPECT_LT(narrow_recipe.scene_separation_vec.at(0).max_separation,
              recipe.scene_separation_vec.at(0).max_separation);
}
