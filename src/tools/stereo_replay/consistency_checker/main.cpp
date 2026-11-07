/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/recipe/recipe_loader.h"
#include "command_line.h"
#include "comparison.h"
#include "frame_hash_sink.h"
#include "overlay_probe_mode.h"
#include "profile_mode.h"
#include "replay_runner.h"
#include "screenshot_mode.h"
#include "types.h"
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>

int main(int   in_argc,
         char* in_argv[])
{
    std::vector<std::string>                                        argument_vec;
    Stereo::Replay::ConsistencyChecker::ExitCode                    exit_code       = Stereo::Replay::ConsistencyChecker::ExitCode::SUCCESS;
    Stereo::Replay::ConsistencyChecker::Options                     options;
    std::string                                                     parse_error;
    std::vector<Stereo::Replay::ConsistencyChecker::FrameHashSink>  sink_vec;
    std::vector<const Stereo::Replay::ConsistencyChecker::Variant*> variant_ptr_vec;

    for (int n_argument = 1;
             n_argument < in_argc;
           ++n_argument)
    {
        argument_vec.push_back(in_argv[n_argument]);
    }

    if (Stereo::Replay::ConsistencyChecker::CommandLine::parse(argument_vec,
                                                               &options,
                                                               &parse_error) == false)
    {
        fprintf(stdout,
                "%s\n\n",
                parse_error.c_str() );
        fprintf(stdout,
                "%s",
                Stereo::Replay::ConsistencyChecker::CommandLine::get_usage_ptr() );

        exit_code = Stereo::Replay::ConsistencyChecker::ExitCode::BAD_ARGUMENTS;

        goto end;
    }

    if (options.recipe_filename.empty() == false)
    {
        std::string error;

        if (Analysis::Recipe::RecipeLoader::load(options.recipe_filename,
                                                 &options.recipe,
                                                 &error) == false)
        {
            fprintf(stdout,
                    "The recipe cannot be read: %s\n",
                    error.c_str() );

            exit_code = Stereo::Replay::ConsistencyChecker::ExitCode::BAD_ARGUMENTS;

            goto end;
        }
    }

    if (options.is_profiling)
    {
        std::string error;

        if (Stereo::Replay::ConsistencyChecker::take_profile(options,
                                                            &error) == false)
        {
            fprintf(stdout,
                    "%s\n",
                    error.c_str() );

            exit_code = Stereo::Replay::ConsistencyChecker::ExitCode::REPLAY_FAILED;
        }

        goto end;
    }

    if (options.same_frames_workload_filename.empty() == false)
    {
        bool        has_difference = false;
        std::string error;

        if (Stereo::Replay::ConsistencyChecker::compare_workloads(options,
                                                                 &has_difference,
                                                                 &error) == false)
        {
            fprintf(stdout,
                    "%s\n",
                    error.c_str() );

            exit_code = Stereo::Replay::ConsistencyChecker::ExitCode::REPLAY_FAILED;

            goto end;
        }

        if (has_difference)
        {
            exit_code = Stereo::Replay::ConsistencyChecker::ExitCode::DIFFERENCES_FOUND;
        }

        goto end;
    }

    if (options.contact_sheet_n_frames != 0)
    {
        std::string error;

        if (Stereo::Replay::ConsistencyChecker::take_contact_sheet(options,
                                                                  &error) == false)
        {
            fprintf(stdout,
                    "%s\n",
                    error.c_str() );

            exit_code = Stereo::Replay::ConsistencyChecker::ExitCode::REPLAY_FAILED;

            goto end;
        }

        fprintf(stdout,
                "Written to %s\n",
                options.image_filename.c_str() );

        goto end;
    }

    if (options.overlay_probe_filename.empty() == false)
    {
        const bool has_dump_directory  = (options.dump_directory.empty()  == false);
        const bool has_recipe_filename = (options.recipe_filename.empty() == false);
        std::string error;

        if (has_recipe_filename == false ||
            has_dump_directory  == false)
        {
            fprintf(stdout,
                    "--overlay-probe needs --recipe and --dump-dir.\n");

            exit_code = Stereo::Replay::ConsistencyChecker::ExitCode::BAD_ARGUMENTS;

            goto end;
        }

        if (Stereo::Replay::ConsistencyChecker::take_overlay_probe(options,
                                                                  &error) == false)
        {
            fprintf(stdout,
                    "%s\n",
                    error.c_str() );

            exit_code = Stereo::Replay::ConsistencyChecker::ExitCode::REPLAY_FAILED;

            goto end;
        }

        fprintf(stdout,
                "Written to %s\n",
                options.dump_directory.c_str() );

        goto end;
    }

    if (options.screenshot_frame != UINT32_MAX)
    {
        std::string error;

        if (Stereo::Replay::ConsistencyChecker::take_screenshot(options,
                                                               &error) == false)
        {
            fprintf(stdout,
                    "%s\n",
                    error.c_str() );

            exit_code = Stereo::Replay::ConsistencyChecker::ExitCode::REPLAY_FAILED;

            goto end;
        }

        fprintf(stdout,
                "Frame %u written to %s\n",
                options.screenshot_frame,
                options.image_filename.c_str() );

        goto end;
    }

    /* The frames as they are come first: they are what the other ways are compared with. Without a choice, the next one tells how much a replay
     * differs from itself; then either the two with executions twice, or the one with the recipe.
     */
    variant_ptr_vec.push_back(&Stereo::Replay::ConsistencyChecker::VARIANT_ARRAY[0]);

    if (options.way_name_vec.empty() )
    {
        variant_ptr_vec.push_back(&Stereo::Replay::ConsistencyChecker::VARIANT_ARRAY[1]);

        if (options.recipe_filename.empty() )
        {
            variant_ptr_vec.push_back(&Stereo::Replay::ConsistencyChecker::VARIANT_ARRAY[2]);
            variant_ptr_vec.push_back(&Stereo::Replay::ConsistencyChecker::VARIANT_ARRAY[3]);
        }
        else
        {
            variant_ptr_vec.push_back(&Stereo::Replay::ConsistencyChecker::VARIANT_ARRAY[4]);
        }
    }
    else
    {
        for (const auto& way_name : options.way_name_vec)
        {
            const bool  has_recipe_filename = (options.recipe_filename.empty() == false);
            const auto* variant_ptr         = Stereo::Replay::ConsistencyChecker::get_variant_ptr(way_name);

            if (variant_ptr == nullptr                                    ||
                (variant_ptr->uses_recipe &&
                 has_recipe_filename == false) )
            {
                fprintf(stdout,
                        "The way \"%s\" is not one that can be replayed here: again, twice, restore, or recipe with --recipe.\n",
                        way_name.c_str() );

                exit_code = Stereo::Replay::ConsistencyChecker::ExitCode::BAD_ARGUMENTS;

                goto end;
            }

            variant_ptr_vec.push_back(variant_ptr);
        }
    }

    {
        std::string error;

        if (Stereo::Replay::ConsistencyChecker::run_variants(options,
                                                             variant_ptr_vec,
                                                            &sink_vec,
                                                            &error) == false)
        {
            fprintf(stdout,
                    "%s\n",
                    error.c_str() );

            exit_code = Stereo::Replay::ConsistencyChecker::ExitCode::REPLAY_FAILED;

            goto end;
        }
    }

    if (options.dump_frame != UINT32_MAX)
    {
        const bool has_dump_directory = (options.dump_directory.empty() == false);

        if (has_dump_directory)
        {
            /* The frame is the one that the last way makes: the one under test. */
            Stereo::Replay::ConsistencyChecker::dump_frame(options,
                                                           options.dump_frame,
                                                           *variant_ptr_vec.back() );
        }
    }

    {
        uint32_t     first_frame            = 0;
        bool         has_difference_to_dump = false;
        const size_t n_sinks                = sink_vec.size();

        for (size_t n_variant = 1;
                    n_variant < n_sinks;
                  ++n_variant)
        {
            uint32_t   variant_first_frame = 0;
            const bool has_difference      = Stereo::Replay::ConsistencyChecker::print_comparison(*variant_ptr_vec.at(n_variant),
                                                                                                  sink_vec.at        (0).get_hash_map        (),
                                                                                                  sink_vec.at        (n_variant).get_hash_map(),
                                                                                                 &variant_first_frame);

            /* The last way is the one under test; the ones before it only show how much a replay differs from itself. */
            if (has_difference          &&
                n_variant == n_sinks - 1)
            {
                first_frame            = variant_first_frame;
                has_difference_to_dump = true;
            }
        }

        if (has_difference_to_dump)
        {
            exit_code = Stereo::Replay::ConsistencyChecker::ExitCode::DIFFERENCES_FOUND;

            if (options.dump_directory.empty() == false)
            {
                Stereo::Replay::ConsistencyChecker::dump_frame(options,
                                                               first_frame,
                                                               *variant_ptr_vec.back() );
            }
        }
    }

end:
    return static_cast<int>(exit_code);
}
