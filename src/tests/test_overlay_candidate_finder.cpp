/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Tests for OverlayCandidateFinder: the draws that the recipe leaves alone, or that the hint of the analysis calls overlays, and where the two disagree.
 */
#include <gtest/gtest.h>
#include "tools/analysis/recipe_bake/command_line.h"
#include "tools/analysis/recipe_bake/overlay_candidate_finder.h"
#include "model_test_helper.h"
#include <algorithm>
#include <string>

namespace
{
    /* Two frames, each with one pass, which draws with another state each: two pass structures of one draw each, and no camera in either. */
    Analysis::APIDumpAnalysis::AnalysisResult make_analysis()
    {
        Analysis::APIDumpAnalysis::AnalysisResult result;
        Tests::ModelTestHelper                    helper;
        const auto                                swapchain_image = helper.create_swapchain_image();

        for (const uint32_t state_key : {1u, 2u})
        {
            helper.bind     (swapchain_image);
            helper.draw     (state_key);
            helper.end_frame();
        }

        result.model = helper.get_model();

        return result;
    }

    bool has_reason(const Analysis::RecipeBake::OverlayCandidate& in_candidate,
                    const std::string&                            in_reason)
    {
        return (std::find(in_candidate.reason_vec.begin(),
                          in_candidate.reason_vec.end  (),
                          in_reason) != in_candidate.reason_vec.end() );
    }
}

TEST(OverlayCandidateFinder, ARecipeThatLeavesNothingAloneAndAnalysisWithoutCamerasHaveNoCandidates)
{
    const auto                           analysis = make_analysis();
    const Analysis::RecipeLoader::Recipe recipe;

    EXPECT_TRUE(Analysis::RecipeBake::OverlayCandidateFinder::find(recipe,
                                                                   analysis).empty() );
}

TEST(OverlayCandidateFinder, ADrawOfAPassTheRecipeLeavesAloneIsACandidateTheHintDoesNotShare)
{
    const auto                     analysis  = make_analysis();
    const auto&                    structure = analysis.model.pass_structure_interner.get_structure(0);
    Analysis::RecipeLoader::Recipe recipe;

    recipe.unaffected.pass_key_vec.push_back(structure.key);

    const auto candidate_vec = Analysis::RecipeBake::OverlayCandidateFinder::find(recipe,
                                                                                  analysis);

    ASSERT_EQ(candidate_vec.size(),
              structure.draw_signature_vec.size() );

    EXPECT_EQ(candidate_vec.at(0).agreement,
              Analysis::RecipeBake::OverlayAgreement::RECIPE_ONLY);
    EXPECT_EQ(candidate_vec.at(0).pass_id,
              0u);
    EXPECT_EQ(candidate_vec.at(0).pass_key,
              structure.key);
    EXPECT_EQ(candidate_vec.at(0).signature_key,
              structure.draw_signature_vec.at(0).signature_key);
    EXPECT_TRUE(candidate_vec.at(0).is_replay_unaffected);
    EXPECT_TRUE(candidate_vec.at(0).has_first_frame);
    EXPECT_EQ(candidate_vec.at(0).first_frame_index,
              0u);
    EXPECT_TRUE(has_reason(candidate_vec.at(0),
                           "recipe_unaffected_pass") );

    /* The converter that plays the application as it runs has no rule for the signature, and plays it as affected: the two ways of asking differ. */
    EXPECT_EQ(candidate_vec.at(0).live_class,
              Analysis::RecipeBake::LiveClass::AFFECTED);
    EXPECT_TRUE(has_reason(candidate_vec.at(0),
                           "replay_and_live_disagree") );
}

TEST(OverlayCandidateFinder, ADrawKeyOfTheRecipeIsToldFromAPassKey)
{
    const auto                     analysis  = make_analysis();
    const auto&                    structure = analysis.model.pass_structure_interner.get_structure(1);
    Analysis::RecipeLoader::Recipe recipe;

    recipe.unaffected.draw_key_vec.push_back(structure.draw_signature_vec.at(0).structure_key);

    const auto candidate_vec = Analysis::RecipeBake::OverlayCandidateFinder::find(recipe,
                                                                                  analysis);

    ASSERT_EQ(candidate_vec.size(),
              1u);
    EXPECT_EQ(candidate_vec.at(0).pass_id,
              1u);
    EXPECT_TRUE(has_reason(candidate_vec.at(0),
                           "recipe_unaffected_draw") );
    EXPECT_FALSE(has_reason(candidate_vec.at(0),
                            "recipe_unaffected_pass") );
}

TEST(OverlayCandidateFinder, ARuleOfASignatureIsWhatTheLiveConverterDoes)
{
    const auto                       analysis  = make_analysis();
    const auto&                      structure = analysis.model.pass_structure_interner.get_structure(0);
    Analysis::RecipeLoader::DrawRule rule;
    Analysis::RecipeLoader::Recipe   recipe;

    rule.draw_class    = Analysis::RecipeLoader::DrawClass::UNAFFECTED;
    rule.signature_key = structure.draw_signature_vec.at(0).signature_key;

    recipe.draw_rule_vec.push_back(rule);

    const auto candidate_vec = Analysis::RecipeBake::OverlayCandidateFinder::find(recipe,
                                                                                  analysis);

    ASSERT_EQ(candidate_vec.size(),
              1u);
    EXPECT_EQ(candidate_vec.at(0).live_class,
              Analysis::RecipeBake::LiveClass::UNAFFECTED);
    EXPECT_FALSE(candidate_vec.at(0).is_replay_unaffected);
    EXPECT_TRUE(has_reason(candidate_vec.at(0),
                           "recipe_rule_unaffected") );
    EXPECT_TRUE(has_reason(candidate_vec.at(0),
                           "replay_and_live_disagree") );
}

TEST(OverlayCandidateFinder, ASkippedDrawIsNotAnOverlay)
{
    const auto                     analysis  = make_analysis();
    const auto&                    structure = analysis.model.pass_structure_interner.get_structure(0);
    Analysis::RecipeLoader::Recipe recipe;

    recipe.skipped.pass_key_vec.push_back(structure.key);

    EXPECT_TRUE(Analysis::RecipeBake::OverlayCandidateFinder::find(recipe,
                                                                   analysis).empty() );
}

TEST(OverlayCandidateFinder, TheJsonHasTheKeysAndWhatTheyAgreeOn)
{
    const auto                     analysis  = make_analysis();
    const auto&                    structure = analysis.model.pass_structure_interner.get_structure(0);
    Analysis::RecipeLoader::Recipe recipe;

    recipe.target_name = "test";

    recipe.unaffected.pass_key_vec.push_back(structure.key);

    Utils::Json::JsonWriter writer(false); /* in_is_pretty */

    Analysis::RecipeBake::OverlayCandidateFinder::write_json(Analysis::RecipeBake::OverlayCandidateFinder::find(recipe,
                                                                                                                 analysis),
                                                             recipe,
                                                            &writer);

    EXPECT_NE(writer.get_text().find("\"agreement\":\"recipe_only\""),
              std::string::npos);
    EXPECT_NE(writer.get_text().find("\"replay_class\":\"unaffected\""),
              std::string::npos);
    EXPECT_NE(writer.get_text().find("\"live_class\":\"affected\""),
              std::string::npos);
    EXPECT_NE(writer.get_text().find("\"recipe\":\"test\""),
              std::string::npos);
}

TEST(OverlayCandidateFinderCommandLine, OverlaysNeedsARecipeAndAPassesFileAndIsNotChecked)
{
    Analysis::RecipeBake::Options options;
    std::string                   error;

    EXPECT_TRUE(Analysis::RecipeBake::CommandLine::parse({"--overlays", "recipe.json", "file.passes"},
                                                        &options,
                                                        &error) );
    EXPECT_TRUE(options.lists_overlays);
    EXPECT_FALSE(options.checks_only);

    EXPECT_FALSE(Analysis::RecipeBake::CommandLine::parse({"--overlays", "recipe.json"},
                                                         &options,
                                                         &error) );
    EXPECT_FALSE(Analysis::RecipeBake::CommandLine::parse({"--overlays", "--check", "recipe.json", "file.passes"},
                                                         &options,
                                                         &error) );
}
