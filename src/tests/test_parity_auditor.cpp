/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Tests for ParityAuditor: the draws on which the replay (by the keys of the pass and of the draw) and the converter that plays the
 * application as it runs (by the signature of the draw and the camera of the pass) get another answer from the recipe.
 */
#include <gtest/gtest.h>
#include "tools/analysis/recipe_bake/command_line.h"
#include "tools/analysis/recipe_bake/parity_auditor.h"
#include "model_test_helper.h"
#include <stdio.h>
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

    Analysis::Recipe::DrawRule make_rule(const Analysis::Recipe::DrawClass& in_draw_class,
                                         const bool&                        in_is_pushed,
                                         const uint64_t&                    in_signature_key)
    {
        Analysis::Recipe::DrawRule result;

        result.draw_class    = in_draw_class;
        result.is_pushed     = in_is_pushed;
        result.signature_key = in_signature_key;

        return result;
    }
}

TEST(ParityAuditor, ARecipeThatLeavesNothingAloneHasNoDifferencesAndTheDrawsAreCounted)
{
    const auto                     analysis = make_analysis();
    const Analysis::Recipe::Recipe recipe;

    const auto report = Analysis::RecipeBake::ParityAuditor::audit(recipe,
                                                                   analysis);

    EXPECT_TRUE(report.difference_vec.empty() );
    EXPECT_EQ  (report.n_draws,
                2u);
    EXPECT_EQ  (report.n_decisions,
                2u);
}

TEST(ParityAuditor, ASignatureRuleThatTheReplayDoesNotHaveIsADifference)
{
    const auto                 analysis  = make_analysis();
    const auto&                structure = analysis.model.pass_structure_interner.get_structure(0);
    const auto&                draw      = structure.draw_signature_vec.at(0);
    Analysis::Recipe::Recipe   recipe;

    recipe.draw_rule_vec.push_back(make_rule(Analysis::Recipe::DrawClass::UNAFFECTED,
                                             false, /* in_is_pushed */
                                             draw.signature_key) );

    const auto report = Analysis::RecipeBake::ParityAuditor::audit(recipe,
                                                                   analysis);

    ASSERT_EQ(report.difference_vec.size(),
              1u);

    const auto& difference = report.difference_vec.at(0);

    EXPECT_EQ(difference.pass_id,
              0u);
    EXPECT_EQ(difference.structure_key,
              draw.structure_key);
    EXPECT_EQ(difference.replay_decision.draw_class,
              Analysis::Recipe::DrawClass::AFFECTED);
    EXPECT_EQ(difference.live_decision.draw_class,
              Analysis::Recipe::DrawClass::UNAFFECTED);
    EXPECT_EQ(difference.n_cameras,
              1u);
    EXPECT_EQ(difference.n_cameras_differing,
              1u);
}

TEST(ParityAuditor, ADrawKeyThatTheConverterCannotTellIsADifference)
{
    const auto               analysis  = make_analysis();
    const auto&              structure = analysis.model.pass_structure_interner.get_structure(1);
    Analysis::Recipe::Recipe recipe;

    recipe.unaffected.draw_key_vec.push_back(structure.draw_signature_vec.at(0).structure_key);

    const auto report = Analysis::RecipeBake::ParityAuditor::audit(recipe,
                                                                   analysis);

    ASSERT_EQ(report.difference_vec.size(),
              1u);
    EXPECT_EQ(report.difference_vec.at(0).pass_id,
              1u);
    EXPECT_EQ(report.difference_vec.at(0).replay_decision.draw_class,
              Analysis::Recipe::DrawClass::UNAFFECTED);
    EXPECT_EQ(report.difference_vec.at(0).live_decision.draw_class,
              Analysis::Recipe::DrawClass::AFFECTED);
}

TEST(ParityAuditor, ADrawLeftAloneBothWaysIsNoDifference)
{
    const auto               analysis  = make_analysis();
    const auto&              structure = analysis.model.pass_structure_interner.get_structure(0);
    const auto&              draw      = structure.draw_signature_vec.at(0);
    Analysis::Recipe::Recipe recipe;

    recipe.draw_rule_vec.push_back(make_rule(Analysis::Recipe::DrawClass::UNAFFECTED,
                                             false, /* in_is_pushed */
                                             draw.signature_key) );
    recipe.unaffected.draw_key_vec.push_back(draw.structure_key);

    EXPECT_TRUE(Analysis::RecipeBake::ParityAuditor::audit(recipe,
                                                           analysis).difference_vec.empty() );
}

TEST(ParityAuditor, APushThatOnlyTheReplayKnowsIsADifferenceEvenIfTheClassIsTheSame)
{
    const auto               analysis  = make_analysis();
    const auto&              structure = analysis.model.pass_structure_interner.get_structure(0);
    Analysis::Recipe::Recipe recipe;

    recipe.pushed.draw_key_vec.push_back(structure.draw_signature_vec.at(0).structure_key);

    const auto report = Analysis::RecipeBake::ParityAuditor::audit(recipe,
                                                                   analysis);

    ASSERT_EQ(report.difference_vec.size(),
              1u);
    EXPECT_EQ(report.difference_vec.at(0).replay_decision.draw_class,
              report.difference_vec.at(0).live_decision.draw_class);
    EXPECT_TRUE (report.difference_vec.at(0).replay_decision.is_pushed);
    EXPECT_FALSE(report.difference_vec.at(0).live_decision.is_pushed);
}

TEST(ParityAuditor, TheReportSaysHowMuchWasComparedAndWhereTheTwoDisagree)
{
    const auto               analysis  = make_analysis();
    const auto&              structure = analysis.model.pass_structure_interner.get_structure(0);
    Analysis::Recipe::Recipe recipe;

    recipe.draw_rule_vec.push_back(make_rule(Analysis::Recipe::DrawClass::UNAFFECTED,
                                             false, /* in_is_pushed */
                                             structure.draw_signature_vec.at(0).signature_key) );

    const auto report = Analysis::RecipeBake::ParityAuditor::audit(recipe,
                                                                   analysis);
    FILE*      file   = ::tmpfile();

    ASSERT_NE(file,
              nullptr);

    Analysis::RecipeBake::ParityAuditor::print_report(file,
                                                      report);

    ::rewind(file);

    std::string text;
    char        buffer_i8vec256[256] = {};

    while (::fgets(buffer_i8vec256,
                   static_cast<int>(sizeof(buffer_i8vec256) ),
                   file) != nullptr)
    {
        text += buffer_i8vec256;
    }

    ::fclose(file);

    EXPECT_NE(text.find("2 draws"),
              std::string::npos) << text;
    EXPECT_NE(text.find("1 differ"),
              std::string::npos) << text;
    EXPECT_NE(text.find("the replay says affected, the converter says unaffected"),
              std::string::npos) << text;
}

TEST(ParityAuditorCommandLine, ParityNeedsARecipeAndAPassesFileAndIsNotChecked)
{
    std::string                         error;
    Analysis::RecipeBake::Options       options;

    ASSERT_TRUE(Analysis::RecipeBake::CommandLine::parse({"--parity", "recipe.json", "file.passes"},
                                                         &options,
                                                         &error) ) << error;

    EXPECT_TRUE(options.audits_parity);
    EXPECT_FALSE(options.checks_only);

    EXPECT_FALSE(Analysis::RecipeBake::CommandLine::parse({"--parity", "--check", "recipe.json", "file.passes"},
                                                          &options,
                                                          &error) );
    EXPECT_FALSE(Analysis::RecipeBake::CommandLine::parse({"--parity", "recipe.json"},
                                                          &options,
                                                          &error) );
}
