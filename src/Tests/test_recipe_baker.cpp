/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Tests for RecipeBaker: what a converter which plays an application as it runs can know of each draw is worked out from the passes
 * the recipe names, and a recipe that cannot be baked is refused with the reason.
 */
#include <gtest/gtest.h>
#include "tools/Analysis/RecipeBake/recipe_baker.h"
#include "Analysis/RecipeLoader/recipe_loader.h"
#include "model_test_helper.h"
#include <algorithm>
#include <stdio.h>
#include <string.h>

class RecipeBakerTest : public ::testing::Test
{
protected:
    /* Two passes into the same target which draw the same: the first with the camera of the first marker, the second with that of
     * the second marker. The state of the draws is the given one, for each.
     */
    static Analysis::APIDumpAnalysis::AnalysisResult make_two_pass_analysis(const uint8_t&  in_first_marker,
                                                            const uint32_t& in_first_state,
                                                            const uint8_t&  in_second_marker,
                                                            const uint32_t& in_second_state,
                                                            const bool&     in_sets_view_matrices,
                                                            uint64_t*       out_second_pass_key_ptr)
    {
        Analysis::APIDumpAnalysis::AnalysisResult result;
        Tests::ModelTestHelper    helper;
        const auto                swapchain_image = helper.create_swapchain_image();

        helper.bind(swapchain_image);

        if (in_sets_view_matrices)
        {
            helper.set_view_matrix(in_first_marker);
        }

        helper.draw(in_first_state);
        helper.bind(swapchain_image);

        if (in_sets_view_matrices)
        {
            helper.set_view_matrix(in_second_marker);
        }

        helper.draw     (in_second_state);
        helper.end_frame();

        *out_second_pass_key_ptr = helper.get_ordered_pass_keys(0).at(1);
        result.model             = helper.get_model();

        return result;
    }

    static std::string make_key_text(const uint64_t& in_key)
    {
        char buffer_i8vec32[32];

        ::snprintf(buffer_i8vec32,
                   sizeof(buffer_i8vec32),
                   "0x%016llx",
                   static_cast<unsigned long long>(in_key) );

        return buffer_i8vec32;
    }

    /* A recipe which says that the pass is unaffected. */
    static std::string make_recipe_text(const uint64_t& in_unaffected_pass_key)
    {
        return "{\"recipe_version\": 0, \"key_scheme_version\": 1, \"target\": {\"name\": \"test\", \"description\": \"a test\"},"
               "\"unaffected\": {\"pass_keys\": [\"" + make_key_text(in_unaffected_pass_key) + "\"]}, \"rules\": []}";
    }
};

TEST_F(RecipeBakerTest, SignaturesOfOneClassGetOneRuleEach)
{
    Analysis::RecipeBake::BakeReport report;
    std::string                      error;
    uint64_t                         second_pass_key = 0;
    const auto                       analysis        = make_two_pass_analysis(1, 1,
                                                                            1, 2,
                                                                            false,
                                                                           &second_pass_key);
    std::string                    text;
    Analysis::RecipeLoader::Recipe recipe;

    ASSERT_TRUE(Analysis::RecipeBake::RecipeBaker::bake(make_recipe_text(second_pass_key),
                                                        analysis,
                                                       &text,
                                                       &report,
                                                       &error) ) << error;
    ASSERT_TRUE(Analysis::RecipeLoader::RecipeLoader::parse(text,
                                                           &recipe,
                                                           &error) ) << error;

    EXPECT_EQ(report.n_signatures,
              2u);
    EXPECT_EQ(report.n_ambiguous_signatures,
              0u);
    EXPECT_EQ(report.n_rules,
              1u);

    /* The draws of the pass which is not named are affected, which needs no rule. */
    ASSERT_EQ(recipe.draw_rule_vec.size(),
              1u);
    EXPECT_EQ  (recipe.draw_rule_vec.at(0).draw_class,
                Analysis::RecipeLoader::DrawClass::UNAFFECTED);
    EXPECT_TRUE(recipe.draw_rule_vec.at(0).view_matrix_vec.empty() );
}

TEST_F(RecipeBakerTest, ASignatureThatPassesOfDifferentClassesDrawIsToldApartByTheViewMatrix)
{
    Analysis::RecipeBake::BakeReport report;
    std::string                      error;
    uint64_t                         second_pass_key = 0;
    const auto                       analysis        = make_two_pass_analysis(1, 5,
                                                                            2, 5,
                                                                            true,
                                                                           &second_pass_key);
    std::string                    text;
    Analysis::RecipeLoader::Recipe recipe;

    ASSERT_TRUE(Analysis::RecipeBake::RecipeBaker::bake(make_recipe_text(second_pass_key),
                                                        analysis,
                                                       &text,
                                                       &report,
                                                       &error) ) << error;
    ASSERT_TRUE(Analysis::RecipeLoader::RecipeLoader::parse(text,
                                                           &recipe,
                                                           &error) ) << error;

    EXPECT_EQ(report.n_ambiguous_signatures,
              1u);
    EXPECT_EQ(report.n_conditional_rules,
              1u);

    /* Only the camera of the unaffected pass is named: the other is what no rule applies to. */
    ASSERT_EQ(recipe.draw_rule_vec.size(),
              1u);
    EXPECT_EQ(recipe.draw_rule_vec.at(0).draw_class,
              Analysis::RecipeLoader::DrawClass::UNAFFECTED);
    ASSERT_EQ(recipe.draw_rule_vec.at(0).view_matrix_vec.size(),
              1u);

    {
        const std::array<float, 16> expected_matrix = []()
        {
            std::array<float, 16> matrix;
            const uint8_t         bytes_u8vec64[64] = {2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
                                                       2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
                                                       2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
                                                       2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2};

            ::memcpy(matrix.data(),
                     bytes_u8vec64,
                     sizeof(bytes_u8vec64) );

            return matrix;
        }();

        /* Written as text and read back, to the last bit. */
        EXPECT_EQ(recipe.draw_rule_vec.at(0).view_matrix_vec.at(0),
                  expected_matrix);
    }
}

TEST_F(RecipeBakerTest, PassesOfDifferentClassesWithTheSameViewMatrixCannotBeBaked)
{
    Analysis::RecipeBake::BakeReport report;
    std::string                      error;
    uint64_t                         second_pass_key = 0;
    const auto                       analysis        = make_two_pass_analysis(1, 5,
                                                                            1, 5,
                                                                            true,
                                                                           &second_pass_key);
    std::string                    text;

    EXPECT_FALSE(Analysis::RecipeBake::RecipeBaker::bake(make_recipe_text(second_pass_key),
                                                         analysis,
                                                        &text,
                                                        &report,
                                                        &error) );
    EXPECT_NE   (error.find("same view matrix"),
                 std::string::npos) << error;
}

TEST_F(RecipeBakerTest, PassesOfDifferentClassesWithNoViewMatrixCannotBeBaked)
{
    Analysis::RecipeBake::BakeReport report;
    std::string                      error;
    uint64_t                         second_pass_key = 0;
    const auto                       analysis        = make_two_pass_analysis(1, 5,
                                                                            1, 5,
                                                                            false,
                                                                           &second_pass_key);
    std::string                    text;

    EXPECT_FALSE(Analysis::RecipeBake::RecipeBaker::bake(make_recipe_text(second_pass_key),
                                                         analysis,
                                                        &text,
                                                        &report,
                                                        &error) );
    EXPECT_NE   (error.find("no view matrix"),
                 std::string::npos) << error;
}

TEST_F(RecipeBakerTest, BakingKeepsTheRestOfTheRecipeAndCanBeDoneAgain)
{
    Analysis::RecipeBake::BakeReport report;
    std::string                      error;
    uint64_t                         second_pass_key = 0;
    const auto                       analysis        = make_two_pass_analysis(1, 1,
                                                                            1, 2,
                                                                            false,
                                                                           &second_pass_key);
    std::string                    again_text;
    std::string                    first_text;
    Analysis::RecipeLoader::Recipe recipe;

    ASSERT_TRUE(Analysis::RecipeBake::RecipeBaker::bake(make_recipe_text(second_pass_key),
                                                        analysis,
                                                       &first_text,
                                                       &report,
                                                       &error) ) << error;
    ASSERT_TRUE(Analysis::RecipeLoader::RecipeLoader::parse(first_text,
                                                           &recipe,
                                                           &error) ) << error;

    EXPECT_EQ(recipe.target_name,
              "test");
    EXPECT_EQ(recipe.target_description,
              "a test");
    ASSERT_EQ(recipe.unaffected.pass_key_vec.size(),
              1u);
    EXPECT_EQ(recipe.unaffected.pass_key_vec.at(0),
              second_pass_key);

    /* A baked recipe is baked again to the same text. */
    ASSERT_TRUE(Analysis::RecipeBake::RecipeBaker::bake(first_text,
                                                        analysis,
                                                       &again_text,
                                                       &report,
                                                       &error) ) << error;

    EXPECT_EQ(again_text,
              first_text);
}

TEST_F(RecipeBakerTest, ABakedSectionOfAnotherVersionIsReplaced)
{
    Analysis::RecipeBake::BakeReport report;
    std::string                      error;
    uint64_t                         second_pass_key = 0;
    const auto                       analysis        = make_two_pass_analysis(1, 1,
                                                                            1, 2,
                                                                            false,
                                                                           &second_pass_key);
    std::string                    recipe_text     = make_recipe_text(second_pass_key);
    std::string                    text;
    Analysis::RecipeLoader::Recipe recipe;

    recipe_text.insert(1,
                       "\"baked\": {\"signature_scheme_version\": 99, \"draw_rules\": []},");

    ASSERT_TRUE(Analysis::RecipeBake::RecipeBaker::bake(recipe_text,
                                                        analysis,
                                                       &text,
                                                       &report,
                                                       &error) ) << error;
    ASSERT_TRUE(Analysis::RecipeLoader::RecipeLoader::parse(text,
                                                           &recipe,
                                                           &error) ) << error;

    EXPECT_EQ(recipe.draw_rule_vec.size(),
              1u);
}

TEST_F(RecipeBakerTest, ARecipeThatIsNotValidIsRefused)
{
    Analysis::RecipeBake::BakeReport report;
    std::string                      error;
    Analysis::APIDumpAnalysis::AnalysisResult      analysis;
    std::string                    text;

    EXPECT_FALSE(Analysis::RecipeBake::RecipeBaker::bake("{",
                                                         analysis,
                                                        &text,
                                                        &report,
                                                        &error) );
    EXPECT_FALSE(Analysis::RecipeBake::RecipeBaker::bake("[]",
                                                         analysis,
                                                        &text,
                                                        &report,
                                                        &error) );
    EXPECT_FALSE(Analysis::RecipeBake::RecipeBaker::bake("{\"recipe_version\": 0}",
                                                         analysis,
                                                        &text,
                                                        &report,
                                                        &error) );
}

TEST_F(RecipeBakerTest, EverySignatureTheAnalysisSawIsListedAsKnown)
{
    Analysis::RecipeBake::BakeReport report;
    std::string                      error;
    uint64_t                         second_pass_key = 0;
    const auto                       analysis        = make_two_pass_analysis(1, 1,
                                                                            1, 2,
                                                                            false,
                                                                           &second_pass_key);
    std::string                    text;
    Analysis::RecipeLoader::Recipe recipe;

    ASSERT_TRUE(Analysis::RecipeBake::RecipeBaker::bake(make_recipe_text(second_pass_key),
                                                        analysis,
                                                       &text,
                                                       &report,
                                                       &error) ) << error;
    ASSERT_TRUE(Analysis::RecipeLoader::RecipeLoader::parse(text,
                                                           &recipe,
                                                           &error) ) << error;

    EXPECT_EQ(recipe.known_signature_key_vec.size(),
              report.n_signatures);
    EXPECT_EQ(recipe.known_signature_key_vec.size(),
              2u);
    EXPECT_TRUE(std::is_sorted(recipe.known_signature_key_vec.begin(),
                               recipe.known_signature_key_vec.end() ));
}

TEST_F(RecipeBakerTest, AnAffectedSignatureGetsTheMedianDepthOfItsPassesAndAnUnaffectedOneGetsNone)
{
    constexpr size_t N_FLOATS_PER_MATRIX = 16;

    Analysis::APIDumpAnalysis::AnalysisResult                  analysis;
    Tests::ModelTestHelper                     helper;
    const auto                                 swapchain_image = helper.create_swapchain_image();
    std::array<float, N_FLOATS_PER_MATRIX>     model_f32mat    = {1, 0, 0, 0,
                                                                  0, 1, 0, 0,
                                                                  0, 0, 1, 0,
                                                                  0, 0, 10, 1};
    std::array<float, N_FLOATS_PER_MATRIX>     view_f32mat     = {1, 0, 0, 0,
                                                                  0, 1, 0, 0,
                                                                  0, 0, 1, 0,
                                                                  0, 0, 0, 1};
    Analysis::RecipeBake::BakeReport report;
    Analysis::RecipeLoader::Recipe   recipe;
    std::string                      error;
    std::string                      text;

    helper.set_matrix(Analysis::APIDumpAnalysis::ConstantSemantic::VIEW_MATRIX,
                      128,
                      view_f32mat);

    /* The first pass is 10 deep and affected; the second, 40 deep, is unaffected. */
    helper.set_matrix(Analysis::APIDumpAnalysis::ConstantSemantic::MODEL_MATRIX,
                      16384,
                      model_f32mat);
    helper.bind(swapchain_image);
    helper.draw(1, 30);

    model_f32mat.at(14) = 40.0f;

    helper.bind(swapchain_image);
    helper.set_matrix(Analysis::APIDumpAnalysis::ConstantSemantic::MODEL_MATRIX,
                      16384,
                      model_f32mat);
    helper.draw(2, 30);
    helper.end_frame();

    analysis.model = helper.get_model();

    ASSERT_TRUE(Analysis::RecipeBake::RecipeBaker::bake(make_recipe_text(helper.get_ordered_pass_keys(0).at(1) ),
                                                        analysis,
                                                       &text,
                                                       &report,
                                                       &error) ) << error;
    ASSERT_TRUE(Analysis::RecipeLoader::RecipeLoader::parse(text,
                                                           &recipe,
                                                           &error) ) << error;

    EXPECT_EQ(report.n_convergence_distances,
              1u);
    EXPECT_EQ(report.n_signatures_without_depth,
              0u);
    ASSERT_EQ(recipe.convergence_vec.size(),
              1u);
    EXPECT_NEAR(recipe.convergence_vec.at(0).distance,
                10.0f,
                1.0f);
}

TEST_F(RecipeBakerTest, AnAffectedSignatureWithNoDepthIsReportedAndHasNoDistance)
{
    Analysis::RecipeBake::BakeReport report;
    std::string                      error;
    uint64_t                         second_pass_key = 0;
    const auto                       analysis        = make_two_pass_analysis(1, 1,
                                                                            1, 2,
                                                                            false,
                                                                           &second_pass_key);
    std::string                    text;
    Analysis::RecipeLoader::Recipe recipe;

    ASSERT_TRUE(Analysis::RecipeBake::RecipeBaker::bake(make_recipe_text(second_pass_key),
                                                        analysis,
                                                       &text,
                                                       &report,
                                                       &error) ) << error;
    ASSERT_TRUE(Analysis::RecipeLoader::RecipeLoader::parse(text,
                                                           &recipe,
                                                           &error) ) << error;

    /* No matrix was set, so no draw is in front of a camera. */
    EXPECT_EQ(report.n_convergence_distances,
              0u);
    EXPECT_EQ(report.n_signatures_without_depth,
              1u);
    EXPECT_TRUE(recipe.convergence_vec.empty() );
}

TEST_F(RecipeBakerTest, PassesThatShareASignatureShareOneDistanceSoThatTheDrawsOfAPassConvergeTogether)
{
    constexpr size_t N_FLOATS_PER_MATRIX = 16;

    Analysis::APIDumpAnalysis::AnalysisResult              analysis;
    Tests::ModelTestHelper                 helper;
    const auto                             swapchain_image = helper.create_swapchain_image();
    std::array<float, N_FLOATS_PER_MATRIX> model_f32mat    = {1, 0, 0, 0,
                                                              0, 1, 0, 0,
                                                              0, 0, 1, 0,
                                                              0, 0, 10, 1};
    std::array<float, N_FLOATS_PER_MATRIX> view_f32mat     = {1, 0, 0, 0,
                                                              0, 1, 0, 0,
                                                              0, 0, 1, 0,
                                                              0, 0, 0, 1};
    Analysis::RecipeBake::BakeReport report;
    Analysis::RecipeLoader::Recipe   recipe;
    std::string                      error;
    std::string                      text;

    helper.set_matrix(Analysis::APIDumpAnalysis::ConstantSemantic::VIEW_MATRIX,
                      128,
                      view_f32mat);

    /* The first pass draws the signatures 1 and 3, 10 deep; the second pass 1 and 2, 40 deep. The signature 1 is the one they share. */
    helper.set_matrix(Analysis::APIDumpAnalysis::ConstantSemantic::MODEL_MATRIX,
                      16384,
                      model_f32mat);
    helper.bind(swapchain_image);
    helper.draw(1, 30);
    helper.draw(3, 30);

    model_f32mat.at(14) = 40.0f;

    helper.bind(swapchain_image);
    helper.set_matrix(Analysis::APIDumpAnalysis::ConstantSemantic::MODEL_MATRIX,
                      16384,
                      model_f32mat);
    helper.draw(1, 30);
    helper.draw(2, 30);
    helper.end_frame();

    analysis.model = helper.get_model();

    /* A pass that is not one of these two is named unaffected, so that both are affected. */
    ASSERT_TRUE(Analysis::RecipeBake::RecipeBaker::bake(make_recipe_text(0),
                                                        analysis,
                                                       &text,
                                                       &report,
                                                       &error) ) << error;
    ASSERT_TRUE(Analysis::RecipeLoader::RecipeLoader::parse(text,
                                                           &recipe,
                                                           &error) ) << error;

    EXPECT_EQ(report.n_convergence_groups,
              1u);
    ASSERT_EQ(recipe.convergence_vec.size(),
              3u);
    EXPECT_FLOAT_EQ(recipe.convergence_vec.at(1).distance,
                    recipe.convergence_vec.at(0).distance);
    EXPECT_FLOAT_EQ(recipe.convergence_vec.at(2).distance,
                    recipe.convergence_vec.at(0).distance);
}

TEST_F(RecipeBakerTest, EachKindOfFrameGetsTheDistanceOfItsOwnPasses)
{
    constexpr size_t N_FLOATS_PER_MATRIX = 16;

    Analysis::APIDumpAnalysis::AnalysisResult              analysis;
    Tests::ModelTestHelper                 helper;
    const auto                             swapchain_image = helper.create_swapchain_image();
    std::array<float, N_FLOATS_PER_MATRIX> model_f32mat    = {1, 0, 0, 0,
                                                              0, 1, 0, 0,
                                                              0, 0, 1, 0,
                                                              0, 0, 10, 1};
    std::array<float, N_FLOATS_PER_MATRIX> view_f32mat     = {1, 0, 0, 0,
                                                              0, 1, 0, 0,
                                                              0, 0, 1, 0,
                                                              0, 0, 0, 1};
    Analysis::RecipeBake::BakeReport report;
    Analysis::RecipeLoader::Recipe   recipe;
    std::string                      error;
    std::string                      text;

    helper.set_matrix(Analysis::APIDumpAnalysis::ConstantSemantic::VIEW_MATRIX,
                      128,
                      view_f32mat);

    /* A frame of one pass, 10 deep; then a frame of another pass, which is another kind of frame, 40 deep. */
    helper.set_matrix(Analysis::APIDumpAnalysis::ConstantSemantic::MODEL_MATRIX,
                      16384,
                      model_f32mat);
    helper.bind(swapchain_image);
    helper.draw(1, 30);
    helper.end_frame();

    model_f32mat.at(14) = 40.0f;

    helper.set_matrix(Analysis::APIDumpAnalysis::ConstantSemantic::MODEL_MATRIX,
                      16384,
                      model_f32mat);
    helper.bind(swapchain_image);
    helper.draw(2, 30);
    helper.end_frame();

    analysis.model = helper.get_model();

    ASSERT_EQ(analysis.model.frame_shape_interner.get_n_structures(),
              2u);
    ASSERT_TRUE(Analysis::RecipeBake::RecipeBaker::bake(make_recipe_text(0),
                                                        analysis,
                                                       &text,
                                                       &report,
                                                       &error) ) << error;
    ASSERT_TRUE(Analysis::RecipeLoader::RecipeLoader::parse(text,
                                                           &recipe,
                                                           &error) ) << error;

    EXPECT_EQ(report.n_scene_distances,
              2u);
    EXPECT_EQ(report.n_scenes_without_depth,
              0u);
    ASSERT_EQ(recipe.scene_convergence_vec.size(),
              2u);

    for (const auto& scene_convergence : recipe.scene_convergence_vec)
    {
        const bool is_first = (scene_convergence.shape_key == helper.get_shape_key(0) );

        EXPECT_TRUE(is_first || scene_convergence.shape_key == helper.get_shape_key(1) );
        EXPECT_NEAR(scene_convergence.distance,
                    (is_first) ? 10.0f
                               : 40.0f,
                    (is_first) ? 1.0f
                               : 4.0f);
    }
}

TEST_F(RecipeBakerTest, ASceneGetsALimitOfTheSeparationFromItsNearestSurfacesAndTheComfortOfTheRecipe)
{
    constexpr size_t N_FLOATS_PER_MATRIX = 16;

    Analysis::APIDumpAnalysis::AnalysisResult              analysis;
    Tests::ModelTestHelper                 helper;
    const auto                             swapchain_image   = helper.create_swapchain_image();
    std::array<float, N_FLOATS_PER_MATRIX> model_f32mat      = {1, 0, 0, 0,
                                                                0, 1, 0, 0,
                                                                0, 0, 1, 0,
                                                                0, 0, 10, 1};
    std::array<float, N_FLOATS_PER_MATRIX> projection_f32mat = {1, 0, 0,     0,
                                                                0, 1, 0,     0,
                                                                0, 0, 1,     1,
                                                                0, 0, -0.1f, 0};
    std::array<float, N_FLOATS_PER_MATRIX> view_f32mat       = {1, 0, 0, 0,
                                                                0, 1, 0, 0,
                                                                0, 0, 1, 0,
                                                                0, 0, 0, 1};
    Analysis::RecipeBake::BakeReport report;
    Analysis::RecipeLoader::Recipe   recipe;
    std::string                      error;
    std::string                      text;

    helper.set_matrix(Analysis::APIDumpAnalysis::ConstantSemantic::PROJECTION_MATRIX,
                      192,
                      projection_f32mat);
    helper.set_matrix(Analysis::APIDumpAnalysis::ConstantSemantic::VIEW_MATRIX,
                      128,
                      view_f32mat);
    helper.set_matrix(Analysis::APIDumpAnalysis::ConstantSemantic::MODEL_MATRIX,
                      16384,
                      model_f32mat);

    /* The object is 10 away; its nearest vertices are 2 away. */
    helper.bind(swapchain_image);
    helper.draw_with_geometry({{0.0f, 0.0f, -8.0f}},
                              30);
    helper.end_frame();

    analysis.model = helper.get_model();

    /* The limit keeps the picture of the eyes at the nearest surface 0.03 of the width apart: 2 * 0.03 / (1 / 2 - 1 / 10). */
    const std::string recipe_text = "{\"recipe_version\": 0, \"key_scheme_version\": 1, \"target\": {\"name\": \"test\", \"description\": \"a test\"},"
                                    "\"comfort\": {\"max_near_parallax\": 0.03, \"near_fraction\": 0.5},"
                                    "\"unaffected\": {\"pass_keys\": [\"0x0000000000000001\"]}, \"rules\": []}";

    ASSERT_TRUE(Analysis::RecipeBake::RecipeBaker::bake(recipe_text,
                                                        analysis,
                                                       &text,
                                                       &report,
                                                       &error) ) << error;
    ASSERT_TRUE(Analysis::RecipeLoader::RecipeLoader::parse(text,
                                                           &recipe,
                                                           &error) ) << error;

    EXPECT_EQ(report.n_scene_limits,
              1u);
    ASSERT_EQ(recipe.scene_separation_vec.size(),
              1u);
    EXPECT_EQ(recipe.scene_separation_vec.at(0).shape_key,
              helper.get_shape_key(0) );
    EXPECT_NEAR(recipe.scene_separation_vec.at(0).max_separation,
                0.15f,
                0.03f);
    EXPECT_FLOAT_EQ(recipe.comfort.max_near_parallax,
                    0.03f);
}

TEST_F(RecipeBakerTest, WithoutComfortNoSceneHasALimit)
{
    constexpr size_t N_FLOATS_PER_MATRIX = 16;

    Analysis::APIDumpAnalysis::AnalysisResult              analysis;
    Tests::ModelTestHelper                 helper;
    const auto                             swapchain_image = helper.create_swapchain_image();
    std::array<float, N_FLOATS_PER_MATRIX> model_f32mat    = {1, 0, 0, 0,
                                                              0, 1, 0, 0,
                                                              0, 0, 1, 0,
                                                              0, 0, 10, 1};
    std::array<float, N_FLOATS_PER_MATRIX> view_f32mat     = {1, 0, 0, 0,
                                                              0, 1, 0, 0,
                                                              0, 0, 1, 0,
                                                              0, 0, 0, 1};
    Analysis::RecipeBake::BakeReport report;
    Analysis::RecipeLoader::Recipe   recipe;
    std::string                      error;
    std::string                      text;

    helper.set_matrix(Analysis::APIDumpAnalysis::ConstantSemantic::VIEW_MATRIX,
                      128,
                      view_f32mat);
    helper.set_matrix(Analysis::APIDumpAnalysis::ConstantSemantic::MODEL_MATRIX,
                      16384,
                      model_f32mat);
    helper.bind(swapchain_image);
    helper.draw_with_geometry({{0.0f, 0.0f, -8.0f}},
                              30);
    helper.end_frame();

    analysis.model = helper.get_model();

    ASSERT_TRUE(Analysis::RecipeBake::RecipeBaker::bake(make_recipe_text(0),
                                                        analysis,
                                                       &text,
                                                       &report,
                                                       &error) ) << error;
    ASSERT_TRUE(Analysis::RecipeLoader::RecipeLoader::parse(text,
                                                           &recipe,
                                                           &error) ) << error;

    EXPECT_EQ(report.n_scene_limits,
              0u);
    EXPECT_TRUE(recipe.scene_separation_vec.empty() );
    EXPECT_EQ  (recipe.scene_convergence_vec.size(),
                1u);
}

TEST_F(RecipeBakerTest, TheKeysOfTheDrawsThatCoverTheViewAreBakedOnlyForThePassesTheRecipeLeavesAlone)
{
    constexpr size_t N_FLOATS_PER_MATRIX = 16;

    Analysis::APIDumpAnalysis::AnalysisResult              analysis;
    Tests::ModelTestHelper                 helper;
    const auto                             swapchain_image   = helper.create_swapchain_image();
    std::array<float, N_FLOATS_PER_MATRIX> projection_f32mat = {1, 0, 0,     0,
                                                                0, 1, 0,     0,
                                                                0, 0, 1,     1,
                                                                0, 0, -0.1f, 0};
    std::array<float, N_FLOATS_PER_MATRIX> view_f32mat       = {1, 0, 0, 0,
                                                                0, 1, 0, 0,
                                                                0, 0, 1, 0,
                                                                0, 0, 0, 1};

    helper.set_matrix(Analysis::APIDumpAnalysis::ConstantSemantic::PROJECTION_MATRIX,
                      192,
                      projection_f32mat);
    helper.set_matrix(Analysis::APIDumpAnalysis::ConstantSemantic::VIEW_MATRIX,
                      128,
                      view_f32mat);
    helper.bind(swapchain_image);
    helper.draw_with_geometry({{-2.0f, -2.0f, 2.0f},
                               { 2.0f, -2.0f, 2.0f},
                               {-2.0f,  2.0f, 2.0f},
                               { 2.0f,  2.0f, 2.0f}},
                              4);
    helper.end_frame();

    analysis.model = helper.get_model();

    const uint64_t pass_key = helper.get_ordered_pass_keys(0).at(0);

    for (const bool is_pass_left_alone : {true, false})
    {
        Analysis::RecipeBake::BakeReport report;
        Analysis::RecipeLoader::Recipe   recipe;
        std::string                      error;
        std::string                      text;

        ASSERT_TRUE(Analysis::RecipeBake::RecipeBaker::bake(make_recipe_text( (is_pass_left_alone) ? pass_key
                                                                                                  : 0x1234u),
                                                            analysis,
                                                           &text,
                                                           &report,
                                                           &error) ) << error;
        ASSERT_TRUE(Analysis::RecipeLoader::RecipeLoader::parse(text,
                                                               &recipe,
                                                               &error) ) << error;

        /* The mesh of a scene can have the key of a quad of an overlay: the keys of a pass that is moved by nothing do not count. */
        EXPECT_EQ(recipe.covering_draw_key_vec.size(),
                  (is_pass_left_alone) ? 1u
                                       : 0u);
    }
}

TEST_F(RecipeBakerTest, ATextureThatFramesCarryOverIsBakedAsOneThatEachEyeHasACopyOf)
{
    Analysis::RecipeBake::BakeReport report;
    std::string                      error;
    Tests::ModelTestHelper           helper;
    const auto                       swapchain_image = helper.create_swapchain_image();

    /* Two textures are made in the same way; the second is read before it is written to, in a frame which writes to it. */
    const auto first_texture  = helper.create_color_texture();
    const auto second_texture = helper.create_color_texture();

    helper.bind        (swapchain_image);
    helper.draw_textured(second_texture);
    helper.bind        (second_texture);
    helper.draw        ();
    helper.bind        (swapchain_image);
    helper.draw_textured(first_texture);
    helper.end_frame   ();

    Analysis::APIDumpAnalysis::AnalysisResult analysis;
    std::string                               text;
    Analysis::RecipeLoader::Recipe            recipe;

    analysis.model = helper.get_model();

    ASSERT_TRUE(Analysis::RecipeBake::RecipeBaker::bake("{\"recipe_version\": 0, \"key_scheme_version\": 1, \"target\": {\"name\": \"test\", \"description\": \"a test\"}, \"rules\": []}",
                                                        analysis,
                                                       &text,
                                                       &report,
                                                       &error) ) << error;
    ASSERT_TRUE(Analysis::RecipeLoader::RecipeLoader::parse(text,
                                                           &recipe,
                                                           &error) ) << error;

    /* Only the texture that is carried over, and it is told from the one made like it by the order. */
    ASSERT_EQ(recipe.per_eye_texture_vec.size(),
              1u);
    EXPECT_EQ(recipe.per_eye_texture_vec.at(0).ordinal,
              1u);
    EXPECT_EQ(recipe.per_eye_texture_vec.at(0).description_key,
              analysis.model.resource_vec.at(1).match_key);
}
