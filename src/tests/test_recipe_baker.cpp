/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Tests for RecipeBaker: what a converter which plays an application as it runs can know of each draw is worked out from the passes
 * the recipe names, and a recipe that cannot be baked is refused with the reason.
 */
#include <gtest/gtest.h>
#include "tools/analysis/recipe_bake/recipe_baker.h"
#include "analysis/recipe/recipe_loader.h"
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
        Tests::ModelTestHelper                    helper;
        const auto                                swapchain_image = helper.create_swapchain_image();

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
    std::string                      text;
    Analysis::Recipe::Recipe         recipe;

    ASSERT_TRUE(Analysis::RecipeBake::RecipeBaker::bake(make_recipe_text(second_pass_key),
                                                        analysis,
                                                       &text,
                                                       &report,
                                                       &error) ) << error;
    ASSERT_TRUE(Analysis::Recipe::RecipeLoader::parse(text,
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
                Analysis::Recipe::DrawClass::UNAFFECTED);
    EXPECT_TRUE(recipe.draw_rule_vec.at(0).view_matrix_vec.empty() );
}

TEST_F(RecipeBakerTest, ADrawThatIsPushedGetsARuleWhateverItsClassIs)
{
    Analysis::RecipeBake::BakeReport report;
    std::string                      error;
    uint64_t                         second_pass_key = 0;
    const auto                       analysis        = make_two_pass_analysis(1, 1,
                                                                            1, 2,
                                                                            false,
                                                                           &second_pass_key);
    std::string                      text;
    Analysis::Recipe::Recipe         recipe;

    /* The pass is pushed and not unaffected, so its draw is affected and pushed, which no rule would say if it were not pushed. */
    ASSERT_TRUE(Analysis::RecipeBake::RecipeBaker::bake("{\"recipe_version\": 0, \"key_scheme_version\": 1, \"target\": {\"name\": \"test\", \"description\": \"a test\"},"
                                                        "\"pushed\": {\"pass_keys\": [\"" + make_key_text(second_pass_key) + "\"]}, \"rules\": []}",
                                                        analysis,
                                                       &text,
                                                       &report,
                                                       &error) ) << error;
    ASSERT_TRUE(Analysis::Recipe::RecipeLoader::parse(text,
                                                      &recipe,
                                                      &error) ) << error;

    ASSERT_EQ(recipe.draw_rule_vec.size(),
              1u);
    EXPECT_EQ  (recipe.draw_rule_vec.at(0).draw_class,
                Analysis::Recipe::DrawClass::AFFECTED);
    EXPECT_TRUE(recipe.draw_rule_vec.at(0).is_pushed);

    /* Pushed and unaffected: the class is as it was, and the rule says it is pushed. */
    ASSERT_TRUE(Analysis::RecipeBake::RecipeBaker::bake("{\"recipe_version\": 0, \"key_scheme_version\": 1, \"target\": {\"name\": \"test\", \"description\": \"a test\"},"
                                                        "\"unaffected\": {\"pass_keys\": [\"" + make_key_text(second_pass_key) + "\"]},"
                                                        "\"pushed\": {\"pass_keys\": [\"" + make_key_text(second_pass_key) + "\"]}, \"rules\": []}",
                                                        analysis,
                                                       &text,
                                                       &report,
                                                       &error) ) << error;
    ASSERT_TRUE(Analysis::Recipe::RecipeLoader::parse(text,
                                                      &recipe,
                                                      &error) ) << error;

    ASSERT_EQ(recipe.draw_rule_vec.size(),
              1u);
    EXPECT_EQ  (recipe.draw_rule_vec.at(0).draw_class,
                Analysis::Recipe::DrawClass::UNAFFECTED);
    EXPECT_TRUE(recipe.draw_rule_vec.at(0).is_pushed);
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
    std::string                      text;
    Analysis::Recipe::Recipe         recipe;

    ASSERT_TRUE(Analysis::RecipeBake::RecipeBaker::bake(make_recipe_text(second_pass_key),
                                                        analysis,
                                                       &text,
                                                       &report,
                                                       &error) ) << error;
    ASSERT_TRUE(Analysis::Recipe::RecipeLoader::parse(text,
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
              Analysis::Recipe::DrawClass::UNAFFECTED);
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
    std::string                      text;

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
    std::string                      text;

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
    std::string                      again_text;
    std::string                      first_text;
    Analysis::Recipe::Recipe         recipe;

    ASSERT_TRUE(Analysis::RecipeBake::RecipeBaker::bake(make_recipe_text(second_pass_key),
                                                        analysis,
                                                       &first_text,
                                                       &report,
                                                       &error) ) << error;
    ASSERT_TRUE(Analysis::Recipe::RecipeLoader::parse(first_text,
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
    std::string                      recipe_text     = make_recipe_text(second_pass_key);
    std::string                      text;
    Analysis::Recipe::Recipe         recipe;

    recipe_text.insert(1,
                       "\"baked\": {\"signature_scheme_version\": 99, \"draw_rules\": []},");

    ASSERT_TRUE(Analysis::RecipeBake::RecipeBaker::bake(recipe_text,
                                                        analysis,
                                                       &text,
                                                       &report,
                                                       &error) ) << error;
    ASSERT_TRUE(Analysis::Recipe::RecipeLoader::parse(text,
                                                      &recipe,
                                                      &error) ) << error;

    EXPECT_EQ(recipe.draw_rule_vec.size(),
              1u);
}

TEST_F(RecipeBakerTest, ARecipeThatIsNotValidIsRefused)
{
    Analysis::RecipeBake::BakeReport          report;
    std::string                               error;
    Analysis::APIDumpAnalysis::AnalysisResult analysis;
    std::string                               text;

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
    std::string                      text;
    Analysis::Recipe::Recipe         recipe;

    ASSERT_TRUE(Analysis::RecipeBake::RecipeBaker::bake(make_recipe_text(second_pass_key),
                                                        analysis,
                                                       &text,
                                                       &report,
                                                       &error) ) << error;
    ASSERT_TRUE(Analysis::Recipe::RecipeLoader::parse(text,
                                                      &recipe,
                                                      &error) ) << error;

    EXPECT_EQ(recipe.known_signature_key_vec.size(),
              report.n_signatures);
    EXPECT_EQ(recipe.known_signature_key_vec.size(),
              2u);
    EXPECT_TRUE(std::is_sorted(recipe.known_signature_key_vec.begin(),
                               recipe.known_signature_key_vec.end() ));
}

TEST_F(RecipeBakerTest, TheDrawsThatAreFlatAndReachFromOneSideOfThePictureToTheOtherAreListedAsSideReaching)
{
    Analysis::RecipeBake::BakeReport report;
    std::string                      error;
    uint64_t                         second_pass_key = 0;
    auto                             analysis        = make_two_pass_analysis(1, 1,
                                                                            1, 2,
                                                                            false,
                                                                           &second_pass_key);
    std::string                      text;
    Analysis::Recipe::Recipe         recipe;

    /* The edges of the picture are at -1 and 1: a bar from one to the other at one depth is an overlay that spans it. */
    Analysis::APIDumpAnalysis::DrawFacts bar;
    Analysis::APIDumpAnalysis::DrawFacts short_bar;
    Analysis::APIDumpAnalysis::DrawFacts floor;
    Analysis::APIDumpAnalysis::DrawFacts orthographic_band;
    Analysis::APIDumpAnalysis::DrawFacts partly_flat_band;
    Analysis::APIDumpAnalysis::DrawFacts perspective_band;

    bar.n_draws_with_footprint = 1;
    bar.x_min                  = -1.0f;
    bar.x_max                  =  1.0f;

    short_bar.n_draws_with_footprint = 1;
    short_bar.x_min                  = -1.0f;
    short_bar.x_max                  =  0.5f;

    floor.n_draws_with_footprint = 1;
    floor.view_depth_max         = 100.0f;
    floor.view_depth_min         = 1.0f;
    floor.x_min                  = -1.0f;
    floor.x_max                  =  1.0f;

    /* A band of quads that are drawn in an orthographic projection, with the depths of their vertices far apart, which together reach beyond both edges: it is flat, because depth
     * changes nothing of where it lands. The same facts of a perspective draw, or of a kind that was only partly drawn flat, are not.
     */
    orthographic_band.n_draws                 = 3;
    orthographic_band.n_draws_flat_layer      = 3;
    orthographic_band.n_draws_with_footprint  = 3;
    orthographic_band.view_depth_max          = 10.1f;
    orthographic_band.view_depth_min          = 0.1f;
    orthographic_band.x_min                   = -2.8f;
    orthographic_band.x_max                   =  1.6f;

    perspective_band           = orthographic_band;
    perspective_band.n_draws_flat_layer = 0;

    partly_flat_band           = orthographic_band;
    partly_flat_band.n_draws_flat_layer = 2;

    analysis.model.draw_facts_map[Analysis::APIDumpAnalysis::DrawFactsKey(0x1, 0xa1)] = bar;
    analysis.model.draw_facts_map[Analysis::APIDumpAnalysis::DrawFactsKey(0x1, 0xb2)] = short_bar;
    analysis.model.draw_facts_map[Analysis::APIDumpAnalysis::DrawFactsKey(0x1, 0xc3)] = floor;
    analysis.model.draw_facts_map[Analysis::APIDumpAnalysis::DrawFactsKey(0x1, 0xd4)] = orthographic_band;
    analysis.model.draw_facts_map[Analysis::APIDumpAnalysis::DrawFactsKey(0x1, 0xe5)] = perspective_band;
    analysis.model.draw_facts_map[Analysis::APIDumpAnalysis::DrawFactsKey(0x1, 0xf6)] = partly_flat_band;

    ASSERT_TRUE(Analysis::RecipeBake::RecipeBaker::bake(make_recipe_text(second_pass_key),
                                                        analysis,
                                                       &text,
                                                       &report,
                                                       &error) ) << error;
    ASSERT_TRUE(Analysis::Recipe::RecipeLoader::parse(text,
                                                      &recipe,
                                                      &error) ) << error;

    ASSERT_EQ(recipe.side_reaching_draw_key_vec.size(),
              2u);
    EXPECT_EQ(recipe.side_reaching_draw_key_vec.at(0),
              0xa1u);
    EXPECT_EQ(recipe.side_reaching_draw_key_vec.at(1),
              0xd4u);
}

TEST_F(RecipeBakerTest, TheDrawsAreToldApartIntoLayersOverlaysAndDrawsInTheSpaceOfTheEyeByWhatTheyDid)
{
    Analysis::APIDumpAnalysis::AnalysisResult analysis;
    Analysis::APIDumpAnalysis::DrawFacts      background;
    Analysis::APIDumpAnalysis::DrawFacts      copy_reader;
    Analysis::APIDumpAnalysis::DrawFacts      eye_space;
    Analysis::APIDumpAnalysis::DrawFacts      mixed;
    Analysis::APIDumpAnalysis::DrawFacts      overlay;
    Analysis::APIDumpAnalysis::DrawFacts      scenery;

    /* Made in the space of the picture, and before any 3D draw of the frame (a menu, a backdrop): a layer, left as it is. */
    background.n_draws            = 4;
    background.n_draws_flat_layer = 4;

    /* Made in the space of the picture, over a 3D scene, on its own: an overlay. */
    overlay.n_draws                = 6;
    overlay.n_draws_after_3d_draw  = 6;
    overlay.n_draws_flat_layer     = 6;

    /* The same, but it samples a copy of the picture: it works on the picture, so it is a layer and no overlay. */
    copy_reader                               = overlay;
    copy_reader.n_draws_sampling_copied_color = 6;

    /* Over a 3D scene in some draws only: not all of them are overlays, so it is a layer. */
    mixed                         = overlay;
    mixed.n_draws_after_3d_draw   = 5;

    /* Every draw has its object at the camera and its vertices in front of it. */
    eye_space.n_draws                = 3;
    eye_space.n_draws_eye_space      = 3;
    eye_space.n_draws_with_footprint = 3;

    /* The 3D scene: some of its draws have the origin at the camera, and the others do not. */
    scenery.n_draws                = 3;
    scenery.n_draws_eye_space      = 1;
    scenery.n_draws_with_footprint = 3;

    analysis.model.draw_facts_map[Analysis::APIDumpAnalysis::DrawFactsKey(0x1, 0xa1)] = background;
    analysis.model.draw_facts_map[Analysis::APIDumpAnalysis::DrawFactsKey(0x1, 0xb2)] = overlay;
    analysis.model.draw_facts_map[Analysis::APIDumpAnalysis::DrawFactsKey(0x1, 0xc3)] = copy_reader;
    analysis.model.draw_facts_map[Analysis::APIDumpAnalysis::DrawFactsKey(0x1, 0xd4)] = mixed;
    analysis.model.draw_facts_map[Analysis::APIDumpAnalysis::DrawFactsKey(0x1, 0xe5)] = eye_space;
    analysis.model.draw_facts_map[Analysis::APIDumpAnalysis::DrawFactsKey(0x1, 0xf6)] = scenery;

    Analysis::RecipeBake::BakeReport report;
    std::string                      error;
    uint64_t                         second_pass_key = 0;
    std::string                      text;
    Analysis::Recipe::Recipe         recipe;

    /* The facts that the analysis of the test has are kept, and the draws are those of a two pass analysis. */
    auto               pass_analysis = make_two_pass_analysis(1, 1,
                                                              1, 2,
                                                              false,
                                                             &second_pass_key);
    const std::string  recipe_text   = "{\"recipe_version\": 0, \"key_scheme_version\": 1, \"target\": {\"name\": \"test\", \"description\": \"a test\"},"
                                       "\"comfort\": {\"automatic_layers\": true},"
                                       "\"unaffected\": {\"pass_keys\": [\"" + make_key_text(second_pass_key) + "\"]}, \"rules\": []}";

    pass_analysis.model.draw_facts_map = analysis.model.draw_facts_map;

    ASSERT_TRUE(Analysis::RecipeBake::RecipeBaker::bake(recipe_text,
                                                        pass_analysis,
                                                       &text,
                                                       &report,
                                                       &error) ) << error;
    ASSERT_TRUE(Analysis::Recipe::RecipeLoader::parse(text,
                                                      &recipe,
                                                      &error) ) << error;

    ASSERT_EQ(recipe.eye_space_draw_key_vec.size(),
              1u);
    EXPECT_EQ(recipe.eye_space_draw_key_vec.at(0),
              0xe5u);
    ASSERT_EQ(recipe.flat_layer_draw_key_vec.size(),
              3u);
    EXPECT_EQ(recipe.flat_layer_draw_key_vec.at(0),
              0xa1u);
    EXPECT_EQ(recipe.flat_layer_draw_key_vec.at(1),
              0xc3u);
    EXPECT_EQ(recipe.flat_layer_draw_key_vec.at(2),
              0xd4u);
    ASSERT_EQ(recipe.overlay_draw_key_vec.size(),
              1u);
    EXPECT_EQ(recipe.overlay_draw_key_vec.at(0),
              0xb2u);
}

TEST_F(RecipeBakerTest, TheListsOfLayersAreBakedOnlyForARecipeThatAsksForThem)
{
    Analysis::RecipeBake::BakeReport report;
    std::string                      error;
    uint64_t                         second_pass_key = 0;
    auto                             analysis        = make_two_pass_analysis(1, 1,
                                                                            1, 2,
                                                                            false,
                                                                           &second_pass_key);
    std::string                      asking_text;
    Analysis::Recipe::Recipe         asking_recipe;
    std::string                      other_text;
    Analysis::Recipe::Recipe         other_recipe;
    Analysis::APIDumpAnalysis::DrawFacts overlay;

    overlay.n_draws               = 2;
    overlay.n_draws_after_3d_draw = 2;
    overlay.n_draws_flat_layer    = 2;

    analysis.model.draw_facts_map[Analysis::APIDumpAnalysis::DrawFactsKey(0x1, 0xb2)] = overlay;

    const std::string asking_recipe_text = "{\"recipe_version\": 0, \"key_scheme_version\": 1, \"target\": {\"name\": \"test\", \"description\": \"a test\"},"
                                           "\"comfort\": {\"automatic_layers\": true},"
                                           "\"unaffected\": {\"pass_keys\": [\"" + make_key_text(second_pass_key) + "\"]}, \"rules\": []}";

    ASSERT_TRUE(Analysis::RecipeBake::RecipeBaker::bake(asking_recipe_text,
                                                        analysis,
                                                       &asking_text,
                                                       &report,
                                                       &error) ) << error;
    ASSERT_TRUE(Analysis::Recipe::RecipeLoader::parse(asking_text,
                                                      &asking_recipe,
                                                      &error) ) << error;
    ASSERT_TRUE(Analysis::RecipeBake::RecipeBaker::bake(make_recipe_text(second_pass_key),
                                                        analysis,
                                                       &other_text,
                                                       &report,
                                                       &error) ) << error;
    ASSERT_TRUE(Analysis::Recipe::RecipeLoader::parse(other_text,
                                                      &other_recipe,
                                                      &error) ) << error;

    EXPECT_TRUE(asking_recipe.comfort.uses_automatic_layers);
    ASSERT_EQ(asking_recipe.overlay_draw_key_vec.size(),
              1u);
    EXPECT_EQ(asking_recipe.overlay_draw_key_vec.at(0),
              0xb2u);
    EXPECT_TRUE(other_recipe.overlay_draw_key_vec.empty() );
    EXPECT_EQ(other_text.find("overlay_draws"),
              std::string::npos);
}

TEST_F(RecipeBakerTest, AnAffectedSignatureGetsTheMedianDepthOfItsPassesAndAnUnaffectedOneGetsNone)
{
    constexpr size_t N_FLOATS_PER_MATRIX = 16;

    Analysis::APIDumpAnalysis::AnalysisResult analysis;
    Tests::ModelTestHelper                    helper;
    const auto                                swapchain_image = helper.create_swapchain_image();
    std::array<float, N_FLOATS_PER_MATRIX>    model_f32mat    = {1, 0, 0, 0,
                                                                 0, 1, 0, 0,
                                                                 0, 0, 1, 0,
                                                                 0, 0, 10, 1};
    std::array<float, N_FLOATS_PER_MATRIX>    view_f32mat     = {1, 0, 0, 0,
                                                                 0, 1, 0, 0,
                                                                 0, 0, 1, 0,
                                                                 0, 0, 0, 1};
    Analysis::RecipeBake::BakeReport          report;
    Analysis::Recipe::Recipe                  recipe;
    std::string                               error;
    std::string                               text;

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
    ASSERT_TRUE(Analysis::Recipe::RecipeLoader::parse(text,
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
    std::string                      text;
    Analysis::Recipe::Recipe         recipe;

    ASSERT_TRUE(Analysis::RecipeBake::RecipeBaker::bake(make_recipe_text(second_pass_key),
                                                        analysis,
                                                       &text,
                                                       &report,
                                                       &error) ) << error;
    ASSERT_TRUE(Analysis::Recipe::RecipeLoader::parse(text,
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

    Analysis::APIDumpAnalysis::AnalysisResult analysis;
    Tests::ModelTestHelper                    helper;
    const auto                                swapchain_image = helper.create_swapchain_image();
    std::array<float, N_FLOATS_PER_MATRIX>    model_f32mat    = {1, 0, 0, 0,
                                                                 0, 1, 0, 0,
                                                                 0, 0, 1, 0,
                                                                 0, 0, 10, 1};
    std::array<float, N_FLOATS_PER_MATRIX>    view_f32mat     = {1, 0, 0, 0,
                                                                 0, 1, 0, 0,
                                                                 0, 0, 1, 0,
                                                                 0, 0, 0, 1};
    Analysis::RecipeBake::BakeReport          report;
    Analysis::Recipe::Recipe                  recipe;
    std::string                               error;
    std::string                               text;

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
    ASSERT_TRUE(Analysis::Recipe::RecipeLoader::parse(text,
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

    Analysis::APIDumpAnalysis::AnalysisResult analysis;
    Tests::ModelTestHelper                    helper;
    const auto                                swapchain_image = helper.create_swapchain_image();
    std::array<float, N_FLOATS_PER_MATRIX>    model_f32mat    = {1, 0, 0, 0,
                                                                 0, 1, 0, 0,
                                                                 0, 0, 1, 0,
                                                                 0, 0, 10, 1};
    std::array<float, N_FLOATS_PER_MATRIX>    view_f32mat     = {1, 0, 0, 0,
                                                                 0, 1, 0, 0,
                                                                 0, 0, 1, 0,
                                                                 0, 0, 0, 1};
    Analysis::RecipeBake::BakeReport          report;
    Analysis::Recipe::Recipe                  recipe;
    std::string                               error;
    std::string                               text;

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
    ASSERT_TRUE(Analysis::Recipe::RecipeLoader::parse(text,
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

    Analysis::APIDumpAnalysis::AnalysisResult analysis;
    Tests::ModelTestHelper                    helper;
    const auto                                swapchain_image   = helper.create_swapchain_image();
    std::array<float, N_FLOATS_PER_MATRIX>    model_f32mat      = {1, 0, 0, 0,
                                                                   0, 1, 0, 0,
                                                                   0, 0, 1, 0,
                                                                   0, 0, 10, 1};
    std::array<float, N_FLOATS_PER_MATRIX>    projection_f32mat = {1, 0, 0,     0,
                                                                   0, 1, 0,     0,
                                                                   0, 0, 1,     1,
                                                                   0, 0, -0.1f, 0};
    std::array<float, N_FLOATS_PER_MATRIX>    view_f32mat       = {1, 0, 0, 0,
                                                                   0, 1, 0, 0,
                                                                   0, 0, 1, 0,
                                                                   0, 0, 0, 1};
    Analysis::RecipeBake::BakeReport          report;
    Analysis::Recipe::Recipe                  recipe;
    std::string                               error;
    std::string                               text;

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
    ASSERT_TRUE(Analysis::Recipe::RecipeLoader::parse(text,
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

namespace
{
    /* A scene whose object is 10 away, with vertices 2 and 50 away: the nearest surfaces and the farthest. */
    Analysis::APIDumpAnalysis::AnalysisResult make_near_and_far_analysis(Tests::ModelTestHelper* inout_helper_ptr)
    {
        constexpr size_t N_FLOATS_PER_MATRIX = 16;

        Analysis::APIDumpAnalysis::AnalysisResult analysis;
        const auto                                swapchain_image   = inout_helper_ptr->create_swapchain_image();
        std::array<float, N_FLOATS_PER_MATRIX>    model_f32mat      = {1, 0, 0, 0,
                                                                       0, 1, 0, 0,
                                                                       0, 0, 1, 0,
                                                                       0, 0, 10, 1};
        std::array<float, N_FLOATS_PER_MATRIX>    projection_f32mat = {2, 0, 0,     0,
                                                                       0, 1, 0,     0,
                                                                       0, 0, 1,     1,
                                                                       0, 0, -0.1f, 0};
        std::array<float, N_FLOATS_PER_MATRIX>    view_f32mat       = {1, 0, 0, 0,
                                                                       0, 1, 0, 0,
                                                                       0, 0, 1, 0,
                                                                       0, 0, 0, 1};

        inout_helper_ptr->set_matrix(Analysis::APIDumpAnalysis::ConstantSemantic::PROJECTION_MATRIX,
                                     192,
                                     projection_f32mat);
        inout_helper_ptr->set_matrix(Analysis::APIDumpAnalysis::ConstantSemantic::VIEW_MATRIX,
                                     128,
                                     view_f32mat);
        inout_helper_ptr->set_matrix(Analysis::APIDumpAnalysis::ConstantSemantic::MODEL_MATRIX,
                                     16384,
                                     model_f32mat);
        inout_helper_ptr->bind(swapchain_image);
        inout_helper_ptr->draw_with_geometry({{0.0f, 0.0f, -8.0f}, {0.0f, 0.0f, 40.0f}},
                                             30);
        inout_helper_ptr->end_frame();

        analysis.model = inout_helper_ptr->get_model();

        return analysis;
    }

    std::string make_comfort_recipe_text(const std::string& in_comfort)
    {
        return "{\"recipe_version\": 0, \"key_scheme_version\": 1, \"target\": {\"name\": \"test\", \"description\": \"a test\"},"
               "\"comfort\": " + in_comfort + ","
               "\"unaffected\": {\"pass_keys\": [\"0x0000000000000001\"]}, \"rules\": []}";
    }
}

TEST_F(RecipeBakerTest, TheFarSurfacesOfASceneLimitTheSeparationToo)
{
    Tests::ModelTestHelper                    helper;
    const auto                                analysis = make_near_and_far_analysis(&helper);
    Analysis::RecipeBake::BakeReport          report;
    Analysis::Recipe::Recipe                  recipe;
    std::string                               error;
    std::string                               text;

    /* Beyond d (10) the picture of the eyes is s * scale * (1 / 10 - 1 / 50) / 2 of the width apart, with the scale of the projection (2): the limit is s = 2 * 0.02 / (2 * 0.08). */
    ASSERT_TRUE(Analysis::RecipeBake::RecipeBaker::bake(make_comfort_recipe_text("{\"max_far_parallax\": 0.02, \"near_fraction\": 0.25}"),
                                                        analysis,
                                                       &text,
                                                       &report,
                                                       &error) ) << error;
    ASSERT_TRUE(Analysis::Recipe::RecipeLoader::parse(text,
                                                      &recipe,
                                                      &error) ) << error;

    ASSERT_EQ(recipe.scene_separation_vec.size(),
              1u);
    EXPECT_GT(recipe.scene_separation_vec.at(0).max_separation,
              0.15f);
    EXPECT_LT(recipe.scene_separation_vec.at(0).max_separation,
              0.35f);
    EXPECT_FLOAT_EQ(recipe.comfort.max_far_parallax,
                    0.02f);
}

TEST_F(RecipeBakerTest, WhatTheRecipeSaysOfASceneWinsOverWhatWasMeasured)
{
    Tests::ModelTestHelper           helper;
    const auto                       analysis = make_near_and_far_analysis(&helper);
    Analysis::RecipeBake::BakeReport report;
    Analysis::Recipe::Recipe         recipe;
    std::string                      error;
    std::string                      text;
    char                             key_i8vec19[19] = {};

    ::snprintf(key_i8vec19,
               sizeof(key_i8vec19),
               "0x%016llx",
               static_cast<unsigned long long>(helper.get_shape_key(0) ) );

    ASSERT_TRUE(Analysis::RecipeBake::RecipeBaker::bake(make_comfort_recipe_text(std::string("{\"max_far_parallax\": 0.02, \"max_near_parallax\": 0.03, \"near_fraction\": 0.25, ") +
                                                                                  "\"scene_overrides\": [{\"shape\": \"" + key_i8vec19 + "\", \"convergence_distance\": 7, \"max_separation\": 0.1}]}"),
                                                        analysis,
                                                       &text,
                                                       &report,
                                                       &error) ) << error;
    ASSERT_TRUE(Analysis::Recipe::RecipeLoader::parse(text,
                                                      &recipe,
                                                      &error) ) << error;

    ASSERT_EQ(recipe.scene_convergence_vec.size(),
              1u);
    EXPECT_FLOAT_EQ(recipe.scene_convergence_vec.at(0).distance,
                    7.0f);
    ASSERT_EQ(recipe.scene_separation_vec.size(),
              1u);
    EXPECT_FLOAT_EQ(recipe.scene_separation_vec.at(0).max_separation,
                    0.1f);
}

TEST_F(RecipeBakerTest, ASceneCanConvergeWhereItsDepthFitsTheTwoLimitsBest)
{
    Tests::ModelTestHelper           helper;
    const auto                       analysis = make_near_and_far_analysis(&helper);
    Analysis::RecipeBake::BakeReport report;
    Analysis::Recipe::Recipe         recipe;
    std::string                      error;
    std::string                      text;

    /* The nearest surface is 2 away and the farthest 50, so 1 / d = 1 / 2 - (1 / 2 - 1 / 50) * 0.03 / (0.03 + 0.02): about 4.7, and the median depth (10) is not used. */
    ASSERT_TRUE(Analysis::RecipeBake::RecipeBaker::bake(make_comfort_recipe_text("{\"balance_convergence\": true, \"max_far_parallax\": 0.02, \"max_near_parallax\": 0.03, \"near_fraction\": 0.25}"),
                                                        analysis,
                                                       &text,
                                                       &report,
                                                       &error) ) << error;
    ASSERT_TRUE(Analysis::Recipe::RecipeLoader::parse(text,
                                                      &recipe,
                                                      &error) ) << error;

    ASSERT_EQ(recipe.scene_convergence_vec.size(),
              1u);
    EXPECT_GT(recipe.scene_convergence_vec.at(0).distance,
              3.0f);
    EXPECT_LT(recipe.scene_convergence_vec.at(0).distance,
              7.0f);

    /* With the eyes there, the nearest surfaces have all of the near limit and the farthest all of the far one: s = 2 * (0.03 + 0.02) / (2 * (1 / 2 - 1 / 50)). */
    ASSERT_EQ(recipe.scene_separation_vec.size(),
              1u);
    EXPECT_GT(recipe.scene_separation_vec.at(0).max_separation,
              0.07f);
    EXPECT_LT(recipe.scene_separation_vec.at(0).max_separation,
              0.15f);
    EXPECT_TRUE(recipe.comfort.balances_convergence);
}

TEST_F(RecipeBakerTest, WithoutComfortNoSceneHasALimit)
{
    constexpr size_t N_FLOATS_PER_MATRIX = 16;

    Analysis::APIDumpAnalysis::AnalysisResult analysis;
    Tests::ModelTestHelper                    helper;
    const auto                                swapchain_image = helper.create_swapchain_image();
    std::array<float, N_FLOATS_PER_MATRIX>    model_f32mat    = {1, 0, 0, 0,
                                                                 0, 1, 0, 0,
                                                                 0, 0, 1, 0,
                                                                 0, 0, 10, 1};
    std::array<float, N_FLOATS_PER_MATRIX>    view_f32mat     = {1, 0, 0, 0,
                                                                 0, 1, 0, 0,
                                                                 0, 0, 1, 0,
                                                                 0, 0, 0, 1};
    Analysis::RecipeBake::BakeReport          report;
    Analysis::Recipe::Recipe                  recipe;
    std::string                               error;
    std::string                               text;

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
    ASSERT_TRUE(Analysis::Recipe::RecipeLoader::parse(text,
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

    Analysis::APIDumpAnalysis::AnalysisResult analysis;
    Tests::ModelTestHelper                    helper;
    const auto                                swapchain_image   = helper.create_swapchain_image();
    std::array<float, N_FLOATS_PER_MATRIX>    projection_f32mat = {1, 0, 0,     0,
                                                                   0, 1, 0,     0,
                                                                   0, 0, 1,     1,
                                                                   0, 0, -0.1f, 0};
    std::array<float, N_FLOATS_PER_MATRIX>    view_f32mat       = {1, 0, 0, 0,
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
        Analysis::Recipe::Recipe         recipe;
        std::string                      error;
        std::string                      text;

        ASSERT_TRUE(Analysis::RecipeBake::RecipeBaker::bake(make_recipe_text( (is_pass_left_alone) ? pass_key
                                                                                                  : 0x1234u),
                                                            analysis,
                                                           &text,
                                                           &report,
                                                           &error) ) << error;
        ASSERT_TRUE(Analysis::Recipe::RecipeLoader::parse(text,
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
    Analysis::Recipe::Recipe                  recipe;

    analysis.model = helper.get_model();

    ASSERT_TRUE(Analysis::RecipeBake::RecipeBaker::bake("{\"recipe_version\": 0, \"key_scheme_version\": 1, \"target\": {\"name\": \"test\", \"description\": \"a test\"}, \"rules\": []}",
                                                        analysis,
                                                       &text,
                                                       &report,
                                                       &error) ) << error;
    ASSERT_TRUE(Analysis::Recipe::RecipeLoader::parse(text,
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

TEST_F(RecipeBakerTest, ASceneWhoseDrawsHaveNoOriginInFrontOfTheCameraGetsItsDistanceFromTheirVertices)
{
    constexpr size_t N_FLOATS_PER_MATRIX = 16;

    Analysis::APIDumpAnalysis::AnalysisResult analysis;
    Tests::ModelTestHelper                    helper;
    const auto                                swapchain_image = helper.create_swapchain_image();
    std::array<float, N_FLOATS_PER_MATRIX>    identity_f32mat = {1, 0, 0, 0,
                                                                 0, 1, 0, 0,
                                                                 0, 0, 1, 0,
                                                                 0, 0, 0, 1};
    Analysis::RecipeBake::BakeReport          report;
    Analysis::Recipe::Recipe                  recipe;
    std::string                               error;
    std::string                               text;

    helper.set_matrix(Analysis::APIDumpAnalysis::ConstantSemantic::VIEW_MATRIX,
                      128,
                      identity_f32mat);
    helper.set_matrix(Analysis::APIDumpAnalysis::ConstantSemantic::MODEL_MATRIX,
                      16384,
                      identity_f32mat);

    /* The origin of the draw is the origin of the eye, not in front of it; its vertices (smoke in the space of the eye) are 20 deep. */
    helper.bind(swapchain_image);
    helper.draw_with_geometry({{0.0f, 0.0f, 20.0f}},
                              30);
    helper.end_frame();

    analysis.model = helper.get_model();

    ASSERT_TRUE(Analysis::RecipeBake::RecipeBaker::bake(make_recipe_text(0),
                                                        analysis,
                                                       &text,
                                                       &report,
                                                       &error) ) << error;
    ASSERT_TRUE(Analysis::Recipe::RecipeLoader::parse(text,
                                                      &recipe,
                                                      &error) ) << error;

    EXPECT_EQ(report.n_scenes_without_depth,
              0u);
    ASSERT_EQ(recipe.scene_convergence_vec.size(),
              1u);
    EXPECT_NEAR(recipe.scene_convergence_vec.at(0).distance,
                20.0f,
                2.0f);
}
