/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Tests for RecipeChecker: the kinds of frames and the draw signatures of an analysis which a baked recipe does not know.
 */
#include <gtest/gtest.h>
#include "Analysis/RecipeBake/recipe_checker.h"
#include "model_test_helper.h"

namespace
{
    /* Three frames: the first and the third draw with one state, the second with another. */
    Analysis::APIDumpAnalysis::AnalysisResult make_analysis(uint64_t* out_first_shape_key_ptr,
                                                            uint64_t* out_second_shape_key_ptr)
    {
        Analysis::APIDumpAnalysis::AnalysisResult result;
        Tests::ModelTestHelper                    helper;
        const auto                                swapchain_image = helper.create_swapchain_image();

        for (const uint32_t state_key : {1u, 2u, 1u})
        {
            helper.bind     (swapchain_image);
            helper.draw     (state_key);
            helper.end_frame();
        }

        *out_first_shape_key_ptr  = helper.get_shape_key(0);
        *out_second_shape_key_ptr = helper.get_shape_key(1);
        result.model              = helper.get_model();

        return result;
    }
}

TEST(RecipeChecker, ARecipeThatListsNothingKnowsEverything)
{
    uint64_t                                  first_shape_key  = 0;
    uint64_t                                  second_shape_key = 0;
    const auto                                analysis         = make_analysis(&first_shape_key,
                                                                               &second_shape_key);
    const Analysis::RecipeLoader::Recipe      recipe;
    const auto                                report           = Analysis::RecipeBake::RecipeChecker::check(recipe,
                                                                                                             analysis);

    EXPECT_EQ(report.n_frames,
              3u);
    EXPECT_EQ(report.n_shapes,
              2u);
    EXPECT_TRUE(report.unknown_shape_vec.empty() );
    EXPECT_TRUE(report.unknown_signature_vec.empty() );
}

TEST(RecipeChecker, AShapeThatTheRecipeDoesNotListIsUnknownWithItsFramesAndWhereItFirstIs)
{
    uint64_t                           first_shape_key  = 0;
    uint64_t                           second_shape_key = 0;
    const auto                         analysis         = make_analysis(&first_shape_key,
                                                                        &second_shape_key);
    Analysis::RecipeLoader::Recipe     recipe;

    recipe.known_shape_key_vec.push_back(first_shape_key);

    const auto report = Analysis::RecipeBake::RecipeChecker::check(recipe,
                                                                   analysis);

    ASSERT_EQ(report.unknown_shape_vec.size(),
              1u);
    EXPECT_EQ(report.unknown_shape_vec.at(0).shape_key,
              second_shape_key);
    EXPECT_EQ(report.unknown_shape_vec.at(0).n_frames,
              1u);
    EXPECT_EQ(report.unknown_shape_vec.at(0).first_frame_index,
              1u);
    EXPECT_EQ(report.n_frames_of_unknown_shapes,
              1u);
}

TEST(RecipeChecker, ASignatureThatTheRecipeDoesNotListIsUnknown)
{
    uint64_t                           first_shape_key  = 0;
    uint64_t                           second_shape_key = 0;
    const auto                         analysis         = make_analysis(&first_shape_key,
                                                                        &second_shape_key);
    Analysis::RecipeLoader::Recipe     recipe;
    std::set<uint64_t>                 signature_key_set;

    for (uint32_t n_pass = 0;
                  n_pass < analysis.model.pass_structure_interner.get_n_structures();
                ++n_pass)
    {
        for (const auto& signature : analysis.model.pass_structure_interner.get_structure(n_pass).draw_signature_vec)
        {
            signature_key_set.insert(signature.signature_key);
        }
    }

    ASSERT_GE(signature_key_set.size(),
              2u); /* the two states are two signatures */

    recipe.known_signature_key_vec.push_back(*signature_key_set.begin() );

    const auto report = Analysis::RecipeBake::RecipeChecker::check(recipe,
                                                                   analysis);

    EXPECT_EQ(report.n_signatures,
              signature_key_set.size() );
    EXPECT_EQ(report.unknown_signature_vec.size(),
              signature_key_set.size() - 1);
    EXPECT_EQ(report.unknown_signature_vec.at(0).n_pass_structures,
              1u);
}
