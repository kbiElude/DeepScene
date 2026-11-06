/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/recipe/recipe_loader.h"
#include "comparison.h"
#include "frame_hash_sink.h"
#include "overlay_probe_mode.h"
#include "profile_mode.h"
#include "replay_runner.h"
#include "screenshot_mode.h"
#include "types.h"
#include "utils/command_line/number_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>
#include <windows.h>

namespace Stereo
{
    namespace Replay
    {
        namespace ConsistencyChecker
        {
            void print_usage();
        }
    }
}

void Stereo::Replay::ConsistencyChecker::print_usage()
{
    fprintf(stdout,
            "Usage:\n"
            "\n"
            "StereoReplayConsistencyChecker_x86 file.workload [--from A] [--to B] [--dump-dir DIR] [--recipe FILE [--eye-separation D]]\n"
            "                                    [--ways again,twice,restore,recipe] [--parallel]\n"
            "StereoReplayConsistencyChecker_x86 file.workload --profile [--from A] [--to B] [--recipe FILE]\n"
            "StereoReplayConsistencyChecker_x86 file.workload --same-frames-as other.workload [--from A] [--to B]\n"
            "StereoReplayConsistencyChecker_x86 file.workload --screenshot N --out FILE.png [--recipe FILE [--eye left|right|both|anaglyph]]\n"
            "StereoReplayConsistencyChecker_x86 file.workload --overlay-probe candidates.json --recipe FILE --dump-dir DIR [--deps FILE] [--eye-separation D]\n"
            "StereoReplayConsistencyChecker_x86 file.workload --contact-sheet K --out FILE.png [--from A] [--to B] [--recipe FILE [--eye ...]]\n"
            "\n"
            "Replays frames A to B (default 0 to 999) of a Direct3D 8 workload in four ways: as they are, as they are again (to\n"
            "see how much a replay differs from itself), executed twice, and executed twice with the state of the device put\n"
            "back before the second execution. The back buffer after each execution is compared with that of the first replay.\n"
            "\n"
            "  --deps FILE      The dependencies of the workload (a .deps file of it, see APIDumpDependencyExtractor): each way goes to the last\n"
            "                   frame as a seek does, leaving out the draws and uploads of the frames which nothing after it reads. For checking\n"
            "                   that it comes out as the replay of every frame does (take two screenshots, with and without, and compare them).\n"
            "  --dump-dir DIR   Write the images of the first frame in which an execution differs, as PNG files\n"
            "  --eye-separation D  The distance between the eyes for the recipe (0 by default)\n"
            "  --recipe FILE    Replay the frames twice with the recipe instead of the last two ways: the first execution is for\n"
            "                   the left eye, the second for the right one. A recipe that changes nothing gives the frames as they are.\n"
            "  --ways LIST      Only these ways, besides the frames as they are (which are always replayed, to compare with): again (as\n"
            "                   they are, again), twice, restore (twice, the state put back between) and recipe (needs --recipe). Each way\n"
            "                   is a replay of the workload from its start, so fewer ways are less to wait for.\n"
            "  --overlay-probe FILE  Instead of the check: FILE is the list of draws that may be 2D overlays (RecipeBake --overlays). The frame in which each is first\n"
            "                   drawn is replayed for the left eye with the draw and without it (the recipe skips it in its pass only); the images go to --dump-dir: an\n"
            "                   animation (GIF) that flips between the frame and the frame with the pixels the draw makes in magenta, and a still, for each candidate that\n"
            "                   makes any, candidates.json with the number of them, and index.html to look at them. A pass which makes no difference left out whole is\n"
            "                   left out for all its draws at once.\n"
            "  --jobs N         With --overlay-probe: share the work between N processes of this program (1 by default: none). Each replay decodes the workload from\n"
            "                   its start with threads of its own, and that is bound by the bandwidth of the memory: one process saturates 8 cores, and two or more\n"
            "                   were no faster (0.9 to 1.0 times). A process takes about 1.5 GB. It may pay on a machine with more memory bandwidth to the cores.\n"
            "  --parallel       Replay the ways at the same time, each with an engine of its own: faster, and needs the memory of all.\n"
            "\n"
            "  --screenshot N   Instead of the check: replay once, up to frame N, and write the image of that frame to the PNG file of --out. With a\n"
            "                   recipe it is both eyes side by side, or the one that --eye says (anaglyph: red-cyan); without one, the frame as it is.\n"
            "  --profile        Instead of the check: replay frames A to B once (as they are, or for the eyes of the recipe) and say where the time of the calls went:\n"
            "                   the functions whose calls took longest, and the frames that took longest.\n"
            "  --same-frames-as FILE  Instead of the check: replay frames A to B of this workload and of FILE as they are, and say whether the images are the\n"
            "                   same (exit code 3 if not). For a slice of a workload (APIDumpSlicer): it has to replay to the images of the workload.\n"
            "  --contact-sheet K  Instead of the check: one PNG with K frames spread evenly from A to B (or the end of the workload), each as a screenshot\n"
            "                   is, and the numbers of the frames in the order they are shown.\n");
}

int main(int   in_argc,
         char* in_argv[])
{
    Stereo::Replay::ConsistencyChecker::ExitCode                    exit_code       = Stereo::Replay::ConsistencyChecker::ExitCode::SUCCESS;
    bool                                                            is_usage_error  = false;
    Stereo::Replay::ConsistencyChecker::Options                     options;
    std::vector<Stereo::Replay::ConsistencyChecker::FrameHashSink>  sink_vec;
    std::vector<const Stereo::Replay::ConsistencyChecker::Variant*> variant_ptr_vec;

    for (int n_argument = 1;
             n_argument < in_argc;
           ++n_argument)
    {
        const std::string argument = in_argv[n_argument];
        const bool        has_next = (n_argument + 1 < in_argc);

        if (argument == "--from" &&
            has_next)
        {
            uint64_t value = 0;

            if (Utils::CommandLine::NumberParser::parse_unsigned(in_argv[++n_argument],
                                                                 UINT32_MAX,
                                                                 &value) == false)
            {
                is_usage_error = true;
            }

            options.first_frame = static_cast<uint32_t>(value);
        }
        else
        if (argument == "--to" &&
            has_next)
        {
            uint64_t value = 0;

            if (Utils::CommandLine::NumberParser::parse_unsigned(in_argv[++n_argument],
                                                                 UINT32_MAX,
                                                                 &value) == false)
            {
                is_usage_error = true;
            }

            options.last_frame = static_cast<uint32_t>(value);
        }
        else
        if (argument == "--contact-sheet" &&
            has_next)
        {
            uint64_t value = 0;

            if (Utils::CommandLine::NumberParser::parse_unsigned(in_argv[++n_argument],
                                                                 UINT32_MAX,
                                                                 &value) == false)
            {
                is_usage_error = true;
            }

            options.contact_sheet_n_frames = static_cast<uint32_t>(value);
        }
        else
        if (argument == "--dump-dir" &&
            has_next)
        {
            options.dump_directory = in_argv[++n_argument];
        }
        else
        if (argument == "--dump-frame" &&
            has_next)
        {
            options.dump_frame = static_cast<uint32_t>(::strtoul(in_argv[++n_argument],
                                                                  nullptr,
                                                                  10) );
        }
        else
        if (argument == "--eye-separation" &&
            has_next)
        {
            double separation = 0.0;

            if (Utils::CommandLine::NumberParser::parse_double(in_argv[++n_argument],
                                                               &separation) == false)
            {
                is_usage_error = true;
            }

            options.eye_separation = static_cast<float>(separation);
        }
        else
        if (argument == "--deps" &&
            has_next)
        {
            options.deps_filename = in_argv[++n_argument];
        }
        else
        if (argument == "--history")
        {
            options.has_history = true;
        }
        else
        if (argument == "--recipe" &&
            has_next)
        {
            options.recipe_filename = in_argv[++n_argument];
        }
        else
        if (argument == "--ways" &&
            has_next)
        {
            std::string list    = in_argv[++n_argument];
            size_t      n_start = 0;

            while (n_start <= list.size() )
            {
                const size_t n_end = list.find(',', n_start);

                options.way_name_vec.push_back(list.substr(n_start,
                                                           (n_end == std::string::npos) ? std::string::npos
                                                                                        : n_end - n_start) );

                n_start = (n_end == std::string::npos) ? list.size() + 1
                                                       : n_end + 1;
            }
        }
        else
        if (argument == "--eye" &&
            has_next)
        {
            const std::string eye_name = in_argv[++n_argument];

            if (eye_name == "left")
            {
                options.eye_view     = Utils::Images::EyeView::LEFT;
                options.has_eye_view = true;
            }
            else
            if (eye_name == "right")
            {
                options.eye_view     = Utils::Images::EyeView::RIGHT;
                options.has_eye_view = true;
            }
            else
            if (eye_name == "both")
            {
                options.eye_view     = Utils::Images::EyeView::BOTH;
                options.has_eye_view = true;
            }
            else
            if (eye_name == "anaglyph")
            {
                options.eye_view     = Utils::Images::EyeView::ANAGLYPH;
                options.has_eye_view = true;
            }
            else
            {
                is_usage_error = true;
            }
        }
        else
        if (argument == "--out" &&
            has_next)
        {
            options.image_filename = in_argv[++n_argument];
        }
        else
        if (argument == "--screenshot" &&
            has_next)
        {
            uint64_t value = 0;

            if (Utils::CommandLine::NumberParser::parse_unsigned(in_argv[++n_argument],
                                                                 UINT32_MAX - 1,
                                                                 &value) == false)
            {
                is_usage_error = true;
            }

            options.screenshot_frame = static_cast<uint32_t>(value);
        }
        else
        if (argument == "--same-frames-as" &&
            has_next)
        {
            options.same_frames_workload_filename = in_argv[++n_argument];
        }
        else
        if (argument == "--profile")
        {
            options.is_profiling = true;
        }
        else
        if (argument == "--overlay-probe" &&
            has_next)
        {
            options.overlay_probe_filename = in_argv[++n_argument];
        }
        else
        if (argument == "--parallel")
        {
            options.is_parallel = true;
        }
        else
        if (argument == "--jobs" &&
            has_next)
        {
            uint64_t value = 0;

            if (Utils::CommandLine::NumberParser::parse_unsigned(in_argv[++n_argument],
                                                                 MAXIMUM_WAIT_OBJECTS - 1,
                                                                 &value) == false ||
                value == 0)
            {
                is_usage_error = true;
            }

            options.n_jobs = static_cast<uint32_t>(value);
        }
        else
        if (argument == "--overlay-probe-shard" &&
            n_argument + 2 < in_argc)
        {
            uint64_t index = 0;
            uint64_t count = 0;

            if (Utils::CommandLine::NumberParser::parse_unsigned(in_argv[++n_argument],
                                                                 MAXIMUM_WAIT_OBJECTS,
                                                                 &index) == false ||
                Utils::CommandLine::NumberParser::parse_unsigned(in_argv[++n_argument],
                                                                 MAXIMUM_WAIT_OBJECTS,
                                                                 &count) == false ||
                index >= count)
            {
                is_usage_error = true;
            }

            options.overlay_probe_shard_count = static_cast<uint32_t>(count);
            options.overlay_probe_shard_index = static_cast<uint32_t>(index);
        }
        else
        if (argument == "--overlay-probe-result" &&
            has_next)
        {
            options.overlay_probe_result_filename = in_argv[++n_argument];
        }
        else
        if (argument.empty()                  == false &&
            argument.at(0)                    != '-'   &&
            options.workload_filename.empty() )
        {
            options.workload_filename = argument;
        }
        else
        {
            is_usage_error = true;
        }
    }

    if (is_usage_error                                         ||
        options.workload_filename.empty()                      ||
        options.first_frame               > options.last_frame ||
        Stereo::Replay::ConsistencyChecker::are_image_options_valid(options) == false)
    {
        Stereo::Replay::ConsistencyChecker::print_usage();

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
        std::string error;

        if (options.recipe_filename.empty() ||
            options.dump_directory.empty() )
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
            const auto* variant_ptr = Stereo::Replay::ConsistencyChecker::get_variant_ptr(way_name);

            if (variant_ptr == nullptr                                                                 ||
                (variant_ptr->uses_recipe && options.recipe_filename.empty() ) )
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

    if (options.dump_frame != UINT32_MAX &&
        options.dump_directory.empty() == false)
    {
        /* The frame is the one that the last way makes: the one under test. */
        Stereo::Replay::ConsistencyChecker::dump_frame(options,
                                                       options.dump_frame,
                                                       *variant_ptr_vec.back() );
    }

    {
        uint32_t first_frame            = 0;
        bool     has_difference_to_dump = false;

        for (size_t n_variant = 1;
                    n_variant < sink_vec.size();
                  ++n_variant)
        {
            uint32_t   variant_first_frame = 0;
            const bool has_difference      = Stereo::Replay::ConsistencyChecker::print_comparison(*variant_ptr_vec.at(n_variant),
                                                                                                  sink_vec.at        (0).get_hash_map        (),
                                                                                                  sink_vec.at        (n_variant).get_hash_map(),
                                                                                                 &variant_first_frame);

            /* The last way is the one under test; the ones before it only show how much a replay differs from itself. */
            if (has_difference                        &&
                n_variant      == sink_vec.size() - 1)
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
