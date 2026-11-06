/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Bakes a recipe (see command_line.cpp for the arguments, and recipe_baker.h for what that is).
 */
#include "Analysis/APIDumpAnalysis/json_reader.h"
#include "Analysis/APIDumpAnalysis/json_writer.h"
#include "Analysis/APIDumpAnalysis/model.h"
#include "Analysis/APIDumpAnalysis/passes_file.h"
#include "Analysis/APIDumpDependencies/deps_file.h"
#include "command_line.h"
#include "overlay_candidate_finder.h"
#include "recipe_baker.h"
#include "recipe_checker.h"
#include "Analysis/RecipeLoader/recipe_loader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>

int main(int   in_argc,
         char* in_argv[])
{
    constexpr int EXIT_BAD_ARGUMENTS      = 1;
    constexpr int EXIT_BAD_INPUT          = 2;
    constexpr int EXIT_CANNOT_BAKE        = 3;
    constexpr int EXIT_CANNOT_WRITE       = 4;
    constexpr int EXIT_NOT_COVERED        = 5;
    constexpr int EXIT_SUSPECT_UNAFFECTED = 6;

    Analysis::APIDumpAnalysis::AnalysisResult analysis;
    std::vector<std::string>                  argument_vec;
    Analysis::APIDumpDependencies::DependencyAnalysis dependencies;
    Analysis::RecipeBake::BakeReport          report;
    std::string                               baked_text;
    std::string                               error;
    FILE*                                     file_ptr     = nullptr;
    bool                                      is_written   = false;
    Analysis::RecipeBake::Options             options;
    std::string                               recipe_text;
    int                                       result       = EXIT_SUCCESS;

    for (int n_argument = 1;
             n_argument < in_argc;
           ++n_argument)
    {
        argument_vec.push_back(in_argv[n_argument]);
    }

    if (Analysis::RecipeBake::CommandLine::parse(argument_vec,
                                                &options,
                                                &error) == false)
    {
        ::fprintf(stderr,
                  "Error: %s\n\n%s",
                  error.c_str(),
                  Analysis::RecipeBake::CommandLine::get_usage_ptr() );

        result = EXIT_BAD_ARGUMENTS;

        goto end;
    }

    if (Analysis::APIDumpAnalysis::JsonReader::read_file(options.recipe_filename,
                                                        &recipe_text) == false)
    {
        ::fprintf(stderr,
                  "Error: cannot read %s\n",
                  options.recipe_filename.c_str() );

        result = EXIT_BAD_INPUT;

        goto end;
    }

    if (Analysis::APIDumpAnalysis::PassesFile::read(options.passes_filename,
                                                   &analysis,
                                                   &error) == false)
    {
        ::fprintf(stderr,
                  "Error: %s: %s\n",
                  options.passes_filename.c_str(),
                  error.c_str() );

        result = EXIT_BAD_INPUT;

        goto end;
    }

    if (options.checks_only ||
        options.lists_overlays)
    {
        Analysis::RecipeLoader::Recipe recipe;

        if (Analysis::RecipeLoader::RecipeLoader::load(options.recipe_filename,
                                                      &recipe,
                                                      &error) == false)
        {
            ::fprintf(stderr,
                      "Error: %s: %s\n",
                      options.recipe_filename.c_str(),
                      error.c_str() );

            result = EXIT_BAD_INPUT;

            goto end;
        }

        if (options.lists_overlays)
        {
            const std::vector<Analysis::RecipeBake::OverlayCandidate> candidate_vec = Analysis::RecipeBake::OverlayCandidateFinder::find(recipe,
                                                                                                                                         analysis);
            Analysis::APIDumpAnalysis::JsonWriter                     writer(true); /* in_is_pretty */

            Analysis::RecipeBake::OverlayCandidateFinder::write_json(candidate_vec,
                                                                     recipe,
                                                                    &writer);

            ::printf("%s\n",
                     writer.get_text().c_str() );

            goto end;
        }

        {
            const auto coverage = Analysis::RecipeBake::RecipeChecker::check(recipe,
                                                                              analysis);

            Analysis::RecipeBake::RecipeChecker::print_report(stdout,
                                                              coverage,
                                                              recipe);

            const bool has_unknown_shape     = (coverage.unknown_shape_vec.empty()     == false);
            const bool has_unknown_signature = (coverage.unknown_signature_vec.empty() == false);

            if (has_unknown_shape     ||
                has_unknown_signature)
            {
                result = EXIT_NOT_COVERED;
            }
            else
            if (coverage.unaffected_warning_vec.empty() == false)
            {
                result = EXIT_SUSPECT_UNAFFECTED;
            }
        }

        goto end;
    }

    if (options.deps_filename.empty() == false                                &&
        Analysis::APIDumpDependencies::DepsFile::read(options.deps_filename,
                                                     &dependencies,
                                                     &error) == false)
    {
        ::fprintf(stderr,
                  "Error: %s: %s\n",
                  options.deps_filename.c_str(),
                  error.c_str() );

        result = EXIT_BAD_INPUT;

        goto end;
    }

    if (Analysis::RecipeBake::RecipeBaker::bake(recipe_text,
                                                analysis,
                                               &baked_text,
                                               &report,
                                               &error,
                                               (options.deps_filename.empty() ) ? nullptr
                                                                                : &dependencies) == false)
    {
        ::fprintf(stderr,
                  "Error: %s\n",
                  error.c_str() );

        result = EXIT_CANNOT_BAKE;

        goto end;
    }

    file_ptr = ::fopen((options.output_filename.empty() ) ? options.recipe_filename.c_str()
                                                          : options.output_filename.c_str(),
                       "wb");

    if (file_ptr != nullptr)
    {
        const size_t n_written = ::fwrite(baked_text.data(),
                                          1,
                                          baked_text.size(),
                                          file_ptr);

        is_written = (n_written == baked_text.size() );
    }

    if (is_written == false)
    {
        ::fprintf(stderr,
                  "Error: cannot write the recipe\n");

        result = EXIT_CANNOT_WRITE;

        goto end;
    }

    ::printf("%u signatures: %u rules, %u of them for signatures which only the view matrix tells apart (%u such signatures)\n",
             report.n_signatures,
             report.n_rules,
             report.n_conditional_rules,
             report.n_ambiguous_signatures);
    ::printf("%u of them have a distance at which the eyes converge (%u distances, as passes which share a signature share one); %u have none, as no draw of theirs was in front of the camera\n",
             report.n_convergence_distances,
             report.n_convergence_groups,
             report.n_signatures_without_depth);
    if (analysis.summary.n_resolution_hazards_map.empty() == false)
    {
        ::printf("The application does what depends on the size of what it renders to, which a render scale above 1 may break (it is stopped if it does):\n");

        for (const auto& hazard_pair : analysis.summary.n_resolution_hazards_map)
        {
            const char* hazard_name_ptr = Analysis::APIDumpAnalysis::get_resolution_hazard_name(hazard_pair.first);

            ::printf("  %s: %llu calls\n",
                     hazard_name_ptr,
                     static_cast<unsigned long long>(hazard_pair.second) );
        }
    }

    ::printf("%u kinds of frames (scenes) have a distance of their own; %u have none, as no affected pass of theirs had a draw in front of the camera\n",
             report.n_scene_distances,
             report.n_scenes_without_depth);
    ::printf("%u of the scenes have a limit of how far apart the eyes may be, from their nearest surfaces\n",
             report.n_scene_limits);
    ::printf("%u textures have a copy for each eye, %s\n",
             report.n_per_eye_textures,
             (report.uses_dependencies) ? "as the dependencies between the frames say"
                                        : "as the totals of the analysis say (a texture which frames use by turns is missed: give --deps)");

end:
    if (file_ptr != nullptr)
    {
        ::fclose(file_ptr);
    }

    return result;
}
