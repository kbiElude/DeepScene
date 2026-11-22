/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/recipe/recipe_loader.h"
#include "command_line.h"
#include "comparison.h"
#include "frame_hash_sink.h"
#include "index_mode.h"
#include "overlay_report_mode.h"
#include "profile_mode.h"
#include "redundant_state_counter_mode.h"
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
    Stereo::Replay::Inspector::ExitCode                             exit_code       = Stereo::Replay::Inspector::ExitCode::SUCCESS;
    Stereo::Replay::Inspector::Options                              options;
    std::string                                                     parse_error;
    std::vector<Stereo::Replay::Inspector::FrameHashSink>           sink_vec;
    std::vector<const Stereo::Replay::Inspector::ReplayWay*> replay_way_ptr_vec;

    for (int n_argument = 1;
             n_argument < in_argc;
           ++n_argument)
    {
        argument_vec.push_back(in_argv[n_argument]);
    }

    if (Stereo::Replay::Inspector::CommandLine::parse(argument_vec,
                                                      &options,
                                                      &parse_error) == false)
    {
        fprintf(stdout,
                "%s\n\n",
                parse_error.c_str() );
        fprintf(stdout,
                "%s",
                Stereo::Replay::Inspector::CommandLine::get_usage_ptr() );

        exit_code = Stereo::Replay::Inspector::ExitCode::BAD_ARGUMENTS;

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

            exit_code = Stereo::Replay::Inspector::ExitCode::BAD_ARGUMENTS;

            goto end;
        }
    }

    if (options.is_making_index)
    {
        std::string error;

        if (Stereo::Replay::Inspector::make_index(options,
                                                 &error) == false)
        {
            fprintf(stdout,
                    "%s\n",
                    error.c_str() );

            exit_code = Stereo::Replay::Inspector::ExitCode::REPLAY_FAILED;
        }

        goto end;
    }

    if (options.is_profiling)
    {
        std::string error;

        if (Stereo::Replay::Inspector::take_profile(options,
                                                   &error) == false)
        {
            fprintf(stdout,
                    "%s\n",
                    error.c_str() );

            exit_code = Stereo::Replay::Inspector::ExitCode::REPLAY_FAILED;
        }

        goto end;
    }

    if (options.is_redundant_state_count)
    {
        std::string error;

        if (Stereo::Replay::Inspector::take_redundant_state_count(options,
                                                        &error) == false)
        {
            fprintf(stdout,
                    "%s\n",
                    error.c_str() );

            exit_code = Stereo::Replay::Inspector::ExitCode::REPLAY_FAILED;
        }

        goto end;
    }

    if (options.same_frames_workload_filename.empty() == false)
    {
        bool        has_difference = false;
        std::string error;

        if (Stereo::Replay::Inspector::compare_workloads(options,
                                                        &has_difference,
                                                        &error) == false)
        {
            fprintf(stdout,
                    "%s\n",
                    error.c_str() );

            exit_code = Stereo::Replay::Inspector::ExitCode::REPLAY_FAILED;

            goto end;
        }

        if (has_difference)
        {
            exit_code = Stereo::Replay::Inspector::ExitCode::DIFFERENCES_FOUND;
        }

        goto end;
    }

    if (options.contact_sheet_n_frames != 0)
    {
        std::string error;

        if (Stereo::Replay::Inspector::take_contact_sheet(options,
                                                         &error) == false)
        {
            fprintf(stdout,
                    "%s\n",
                    error.c_str() );

            exit_code = Stereo::Replay::Inspector::ExitCode::REPLAY_FAILED;

            goto end;
        }

        fprintf(stdout,
                "Written to %s\n",
                options.image_filename.c_str() );

        goto end;
    }

    if (options.overlay_report_filename.empty() == false)
    {
        const bool has_dump_directory  = (options.dump_directory.empty()  == false);
        const bool has_recipe_filename = (options.recipe_filename.empty() == false);
        std::string error;

        if (has_recipe_filename == false ||
            has_dump_directory  == false)
        {
            fprintf(stdout,
                    "--overlay-report needs --recipe and --dump-dir.\n");

            exit_code = Stereo::Replay::Inspector::ExitCode::BAD_ARGUMENTS;

            goto end;
        }

        if (Stereo::Replay::Inspector::take_overlay_report(options,
                                                         &error) == false)
        {
            fprintf(stdout,
                    "%s\n",
                    error.c_str() );

            exit_code = Stereo::Replay::Inspector::ExitCode::REPLAY_FAILED;

            goto end;
        }

        fprintf(stdout,
                "Written to %s\n",
                options.dump_directory.c_str() );

        goto end;
    }

    if (options.screenshot_frame_vec.empty() == false)
    {
        std::string                                  error;
        Stereo::Replay::Inspector::ScreenshotReport report;

        if (Stereo::Replay::Inspector::take_screenshot(options,
                                                      &report,
                                                      &error) == false)
        {
            fprintf(stdout,
                    "%s\n",
                    error.c_str() );

            exit_code = Stereo::Replay::Inspector::ExitCode::REPLAY_FAILED;

            goto end;
        }

        for (const auto& file : report.written_vec)
        {
            fprintf(stdout,
                    "Frame %u written to %s\n",
                    file.frame,
                    file.filename.c_str() );
        }

        if (report.skipped_frame_vec.empty() == false)
        {
            fprintf(stdout,
                    "Not written, the replay did not reach them:");

            for (const auto n_frame : report.skipped_frame_vec)
            {
                fprintf(stdout,
                        " %u",
                        n_frame);
            }

            fprintf(stdout,
                    "\n");
        }

        goto end;
    }

    /* The frames as they are come first; every other way is compared against them. If the user names no ways, the default ways follow. The
     * first is a second replay of the frames, which shows how much a replay differs from itself. Then come either the two ways that execute
     * each frame twice, or, with a recipe, the way that uses the recipe.
     */
    replay_way_ptr_vec.push_back(&Stereo::Replay::Inspector::REPLAY_WAY_ARRAY[0]);

    if (options.way_name_vec.empty() )
    {
        replay_way_ptr_vec.push_back(&Stereo::Replay::Inspector::REPLAY_WAY_ARRAY[1]);

        if (options.recipe_filename.empty() )
        {
            replay_way_ptr_vec.push_back(&Stereo::Replay::Inspector::REPLAY_WAY_ARRAY[2]);
            replay_way_ptr_vec.push_back(&Stereo::Replay::Inspector::REPLAY_WAY_ARRAY[3]);
        }
        else
        {
            replay_way_ptr_vec.push_back(&Stereo::Replay::Inspector::REPLAY_WAY_ARRAY[4]);
        }
    }
    else
    {
        for (const auto& way_name : options.way_name_vec)
        {
            const bool  has_recipe_filename = (options.recipe_filename.empty() == false);
            const auto* variant_ptr         = Stereo::Replay::Inspector::get_replay_way_ptr(way_name);

            if (variant_ptr == nullptr                                    ||
                (variant_ptr->uses_recipe &&
                 has_recipe_filename == false) )
            {
                fprintf(stdout,
                        "The way \"%s\" is not one that can be replayed here: again, twice, restore, or recipe with --recipe.\n",
                        way_name.c_str() );

                exit_code = Stereo::Replay::Inspector::ExitCode::BAD_ARGUMENTS;

                goto end;
            }

            replay_way_ptr_vec.push_back(variant_ptr);
        }
    }

    {
        std::string error;

        if (Stereo::Replay::Inspector::run_variants(options,
                                                    replay_way_ptr_vec,
                                                   &sink_vec,
                                                   &error) == false)
        {
            fprintf(stdout,
                    "%s\n",
                    error.c_str() );

            exit_code = Stereo::Replay::Inspector::ExitCode::REPLAY_FAILED;

            goto end;
        }
    }

    if (options.dump_frame != UINT32_MAX)
    {
        const bool has_dump_directory = (options.dump_directory.empty() == false);

        if (has_dump_directory)
        {
            /* The dumped frame is the one that the last way produces, which is the way under test. */
            Stereo::Replay::Inspector::dump_frame(options,
                                                  options.dump_frame,
                                                  *replay_way_ptr_vec.back() );
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
            const bool has_difference      = Stereo::Replay::Inspector::print_comparison(*replay_way_ptr_vec.at(n_variant),
                                                                                         sink_vec.at        (0).get_hash_map        (),
                                                                                         sink_vec.at        (n_variant).get_hash_map(),
                                                                                        &variant_first_frame);

            /* The last way is the one under test; the earlier ways only show how much a replay differs from itself. */
            if (has_difference          &&
                n_variant == n_sinks - 1)
            {
                first_frame            = variant_first_frame;
                has_difference_to_dump = true;
            }
        }

        if (has_difference_to_dump)
        {
            exit_code = Stereo::Replay::Inspector::ExitCode::DIFFERENCES_FOUND;

            if (options.dump_directory.empty() == false)
            {
                Stereo::Replay::Inspector::dump_frame(options,
                                                      first_frame,
                                                      *replay_way_ptr_vec.back() );
            }
        }
    }

end:
    return static_cast<int>(exit_code);
}
