/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Tests for the recipe baker with the dependencies between the frames: they say which textures each eye is to have a copy of, which the totals of
 * the analysis miss when frames use two textures by turns.
 */
#include <gtest/gtest.h>
#include "tools/analysis/recipe_bake/recipe_baker.h"
#include "analysis/recipe_loader/recipe_loader.h"
#include "dependency_scene.h"
#include "model_test_helper.h"

namespace
{
    const char* const RECIPE_TEXT = "{\"recipe_version\": 0, \"key_scheme_version\": 1, \"target\": {\"name\": \"test\", \"description\": \"a test\"}, \"rules\": []}";

    /* Two targets which are made in the same way, and which frames render to by turns, reading the one that the frame before rendered to. */
    Analysis::APIDumpDependencies::DependencyAnalysis make_ping_pong_dependencies()
    {
        Tests::DependencyScene scene;
        const auto             first  = scene.create_resource(1,
                                                              Analysis::APIDumpAnalysis::ResourceKind::TEXTURE_2D,
                                                              true,
                                                              0xCC);
        const auto             second = scene.create_resource(1,
                                                              Analysis::APIDumpAnalysis::ResourceKind::TEXTURE_2D,
                                                              true,
                                                              0xCC);

        for (uint32_t n_frame = 0;
                      n_frame < 6;
                    ++n_frame)
        {
            const auto written = (n_frame % 2 == 0) ? first  : second;
            const auto read    = (n_frame % 2 == 0) ? second : first;

            scene.sample     (read);
            scene.clear_fully(written);
            scene.draw_to    (written);
            scene.end_frame  ();
        }

        return scene.make_analysis();
    }
}

TEST(RecipeBakerDependencies, TexturesWhichFramesUseByTurnsAreBothBakedAsOnesThatEachEyeHasACopyOf)
{
    Analysis::APIDumpAnalysis::AnalysisResult analysis;
    Analysis::RecipeBake::BakeReport          report;
    Analysis::RecipeLoader::Recipe            recipe;
    std::string                               error;
    std::string                               text;
    const auto                                dependencies = make_ping_pong_dependencies();

    /* The analysis is of the same workload: its size and the hash of its start are all it holds of it. */
    analysis.workload_reference.n_bytes = dependencies.workload_reference.n_bytes;

    ASSERT_TRUE(Analysis::RecipeBake::RecipeBaker::bake(RECIPE_TEXT,
                                                        analysis,
                                                       &text,
                                                       &report,
                                                       &error,
                                                       &dependencies) ) << error;
    ASSERT_TRUE(Analysis::RecipeLoader::RecipeLoader::parse(text,
                                                           &recipe,
                                                           &error) ) << error;

    EXPECT_TRUE(report.uses_dependencies);
    EXPECT_EQ  (report.n_per_eye_textures,
                2u);

    ASSERT_EQ(recipe.per_eye_texture_vec.size(),
              2u);
    EXPECT_EQ(recipe.per_eye_texture_vec.at(0).description_key,
              0xCCu);
    EXPECT_EQ(recipe.per_eye_texture_vec.at(0).ordinal,
              0u);
    EXPECT_EQ(recipe.per_eye_texture_vec.at(1).ordinal,
              1u);
}

TEST(RecipeBakerDependencies, WithoutThemTheTotalsOfTheAnalysisAreWhatSaysSoAndTheReportSaysThat)
{
    Analysis::RecipeBake::BakeReport          report;
    Analysis::APIDumpAnalysis::AnalysisResult analysis;
    std::string                               error;
    std::string                               text;

    ASSERT_TRUE(Analysis::RecipeBake::RecipeBaker::bake(RECIPE_TEXT,
                                                        analysis,
                                                       &text,
                                                       &report,
                                                       &error) ) << error;

    EXPECT_FALSE(report.uses_dependencies);
    EXPECT_EQ   (report.n_per_eye_textures,
                 0u);
}

TEST(RecipeBakerDependencies, DependenciesOfAnotherWorkloadAreRefused)
{
    Analysis::APIDumpAnalysis::AnalysisResult analysis;
    Analysis::RecipeBake::BakeReport          report;
    std::string                               error;
    std::string                               text;
    auto                                      dependencies = make_ping_pong_dependencies();

    analysis.workload_reference.n_bytes = dependencies.workload_reference.n_bytes + 1;

    EXPECT_FALSE(Analysis::RecipeBake::RecipeBaker::bake(RECIPE_TEXT,
                                                         analysis,
                                                        &text,
                                                        &report,
                                                        &error,
                                                        &dependencies) );
    EXPECT_NE(error.find(".deps"),
              std::string::npos);

    /* One which differs only in a hash that both hold. */
    analysis.workload_reference.n_bytes    = dependencies.workload_reference.n_bytes;
    analysis.workload_reference.header_hash = 0x1;
    dependencies.workload_reference.header_hash = 0x2;

    EXPECT_FALSE(Analysis::RecipeBake::RecipeBaker::bake(RECIPE_TEXT,
                                                         analysis,
                                                        &text,
                                                        &report,
                                                        &error,
                                                        &dependencies) );
}
