/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "command_line.h"
#include "utils/command_line/number_parser.h"
#include <stdlib.h>
#include <windows.h>

const char* Stereo::Replay::ConsistencyChecker::CommandLine::get_usage_ptr()
{
    return "Usage:\n"
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
           "                   is, and the numbers of the frames in the order they are shown.\n";
}

bool Stereo::Replay::ConsistencyChecker::CommandLine::parse(const std::vector<std::string>& in_argument_vec,
                                                            Options*                        out_options_ptr,
                                                            std::string*                    out_error_ptr)
{
    /* The options which are a flag, a text or a whole number: the name of each, and the field of the options which it sets. */
    struct FlagOption
    {
        const char*     name_ptr;
        bool Options::* member_ptr;
    };

    struct NumberOption
    {
        const char*         name_ptr;
        uint32_t Options::* member_ptr;
        uint64_t            least;
        uint64_t            most;
    };

    struct TextOption
    {
        const char*            name_ptr;
        std::string Options::* member_ptr;
    };

    static constexpr int DECIMAL_BASE = 10;

    static const FlagOption FLAG_OPTION_ARRAY[] =
    {
        {"--history",  &Options::has_history},
        {"--parallel", &Options::is_parallel},
        {"--profile",  &Options::is_profiling}
    };

    static const NumberOption NUMBER_OPTION_ARRAY[] =
    {
        {"--contact-sheet", &Options::contact_sheet_n_frames, 0, UINT32_MAX},
        {"--from",          &Options::first_frame,            0, UINT32_MAX},
        {"--jobs",          &Options::n_jobs,                 1, MAXIMUM_WAIT_OBJECTS - 1},
        {"--screenshot",    &Options::screenshot_frame,       0, UINT32_MAX - 1},
        {"--to",            &Options::last_frame,             0, UINT32_MAX}
    };

    static const TextOption TEXT_OPTION_ARRAY[] =
    {
        {"--deps",                 &Options::deps_filename},
        {"--dump-dir",             &Options::dump_directory},
        {"--out",                  &Options::image_filename},
        {"--overlay-probe",        &Options::overlay_probe_filename},
        {"--overlay-probe-result", &Options::overlay_probe_result_filename},
        {"--recipe",               &Options::recipe_filename},
        {"--same-frames-as",       &Options::same_frames_workload_filename}
    };

    bool result = false;

    *out_options_ptr = Options();

    for (size_t n_argument = 0;
                n_argument < in_argument_vec.size();
              ++n_argument)
    {
        const std::string&  argument   = in_argument_vec.at(n_argument);
        const FlagOption*   flag_ptr   = nullptr;
        const bool          has_next   = (n_argument + 1 < in_argument_vec.size() );
        const NumberOption* number_ptr = nullptr;
        const TextOption*   text_ptr   = nullptr;

        for (const auto& option : FLAG_OPTION_ARRAY)
        {
            if (argument == option.name_ptr)
            {
                flag_ptr = &option;
            }
        }

        for (const auto& option : NUMBER_OPTION_ARRAY)
        {
            if (argument == option.name_ptr)
            {
                number_ptr = &option;
            }
        }

        for (const auto& option : TEXT_OPTION_ARRAY)
        {
            if (argument == option.name_ptr)
            {
                text_ptr = &option;
            }
        }

        {
            const bool needs_value = (number_ptr != nullptr            ||
                                      text_ptr   != nullptr            ||
                                      argument   == "--dump-frame"     ||
                                      argument   == "--eye"            ||
                                      argument   == "--eye-separation" ||
                                      argument   == "--ways");

            if (needs_value &&
                has_next == false)
            {
                *out_error_ptr = argument + " needs a value";

                goto end;
            }
        }

        if (argument == "--overlay-probe-shard" &&
            n_argument + 2 >= in_argument_vec.size() )
        {
            *out_error_ptr = argument + " needs the number of the part and the number of the parts";

            goto end;
        }

        if (flag_ptr != nullptr)
        {
            out_options_ptr->*(flag_ptr->member_ptr) = true;
        }
        else
        if (number_ptr != nullptr)
        {
            const std::string& value_text = in_argument_vec.at(++n_argument);

            if (parse_unsigned(value_text,
                               number_ptr->least,
                               number_ptr->most,
                               &(out_options_ptr->*(number_ptr->member_ptr) ) ) == false)
            {
                *out_error_ptr = argument + " needs a whole number from " + std::to_string(number_ptr->least) + " to " + std::to_string(number_ptr->most);

                goto end;
            }
        }
        else
        if (text_ptr != nullptr)
        {
            out_options_ptr->*(text_ptr->member_ptr) = in_argument_vec.at(++n_argument);
        }
        else
        if (argument == "--dump-frame")
        {
            const std::string& value_text = in_argument_vec.at(++n_argument);

            out_options_ptr->dump_frame = static_cast<uint32_t>(::strtoul(value_text.c_str(),
                                                                          nullptr,
                                                                          DECIMAL_BASE) );
        }
        else
        if (argument == "--eye")
        {
            const std::string& value_text = in_argument_vec.at(++n_argument);

            if (parse_eye_view(value_text,
                               &out_options_ptr->eye_view) == false)
            {
                *out_error_ptr = "--eye is left, right, both or anaglyph";

                goto end;
            }

            out_options_ptr->has_eye_view = true;
        }
        else
        if (argument == "--eye-separation")
        {
            double             separation = 0.0;
            const std::string& value_text = in_argument_vec.at(++n_argument);

            if (Utils::CommandLine::NumberParser::parse_double(value_text,
                                                               &separation) == false)
            {
                *out_error_ptr = "--eye-separation needs a number";

                goto end;
            }

            out_options_ptr->eye_separation = static_cast<float>(separation);
        }
        else
        if (argument == "--overlay-probe-shard")
        {
            const std::string& count_text = in_argument_vec.at(n_argument + 2);
            const std::string& index_text = in_argument_vec.at(n_argument + 1);

            if (parse_unsigned(index_text,
                               0,
                               MAXIMUM_WAIT_OBJECTS,
                               &out_options_ptr->overlay_probe_shard_index) == false ||
                parse_unsigned(count_text,
                               0,
                               MAXIMUM_WAIT_OBJECTS,
                               &out_options_ptr->overlay_probe_shard_count) == false ||
                out_options_ptr->overlay_probe_shard_index >= out_options_ptr->overlay_probe_shard_count)
            {
                *out_error_ptr = argument + " needs the number of the part, and the number of the parts (which is higher)";

                goto end;
            }

            n_argument += 2;
        }
        else
        if (argument == "--ways")
        {
            const std::string&             value_text   = in_argument_vec.at(++n_argument);
            const std::vector<std::string> way_name_vec = split_list(value_text);

            out_options_ptr->way_name_vec.insert(out_options_ptr->way_name_vec.end(),
                                                 way_name_vec.begin(),
                                                 way_name_vec.end() );
        }
        else
        if (argument.empty()                           == false &&
            argument.at(0)                             != '-'   &&
            out_options_ptr->workload_filename.empty() )
        {
            out_options_ptr->workload_filename = argument;
        }
        else
        {
            *out_error_ptr = "Not an option, or a second workload: " + argument;

            goto end;
        }
    }

    if (out_options_ptr->workload_filename.empty() )
    {
        *out_error_ptr = "A workload file is needed";

        goto end;
    }

    if (out_options_ptr->first_frame > out_options_ptr->last_frame)
    {
        *out_error_ptr = "--from is after --to";

        goto end;
    }

    if (are_image_options_valid(*out_options_ptr) == false)
    {
        *out_error_ptr = "A screenshot or a contact sheet needs --out (and --eye needs a recipe); --out and --eye are only for one of them, and not for both at once";

        goto end;
    }

    result = true;

end:
    return result;
}

bool Stereo::Replay::ConsistencyChecker::CommandLine::are_image_options_valid(const Options& in_options)
{
    const bool is_contact_sheet = (in_options.contact_sheet_n_frames != 0);
    const bool is_screenshot    = (in_options.screenshot_frame       != UINT32_MAX);
    bool       result           = false;

    if (is_contact_sheet &&
        is_screenshot)
    {
        goto end;
    }

    if (is_contact_sheet ||
        is_screenshot)
    {
        result = (in_options.image_filename.empty() == false &&
                  (in_options.has_eye_view == false          ||
                   in_options.recipe_filename.empty() == false) );
    }
    else
    {
        result = (in_options.image_filename.empty() &&
                  in_options.has_eye_view == false);
    }

end:
    return result;
}

bool Stereo::Replay::ConsistencyChecker::CommandLine::parse_eye_view(const std::string&      in_name,
                                                                     Utils::Images::EyeView* out_view_ptr)
{
    struct NamedView
    {
        const char*            name_ptr;
        Utils::Images::EyeView view;
    };

    static const NamedView NAMED_VIEW_ARRAY[] =
    {
        {"anaglyph", Utils::Images::EyeView::ANAGLYPH},
        {"both",     Utils::Images::EyeView::BOTH},
        {"left",     Utils::Images::EyeView::LEFT},
        {"right",    Utils::Images::EyeView::RIGHT}
    };

    bool result = false;

    for (const auto& named_view : NAMED_VIEW_ARRAY)
    {
        if (in_name == named_view.name_ptr)
        {
            *out_view_ptr = named_view.view;
            result        = true;
        }
    }

    return result;
}

bool Stereo::Replay::ConsistencyChecker::CommandLine::parse_unsigned(const std::string& in_text,
                                                                     const uint64_t&    in_least,
                                                                     const uint64_t&    in_most,
                                                                     uint32_t*          out_value_ptr)
{
    bool     result = false;
    uint64_t value  = 0;

    if (Utils::CommandLine::NumberParser::parse_unsigned(in_text,
                                                         in_most,
                                                         &value) == false)
    {
        goto end;
    }

    if (value < in_least)
    {
        goto end;
    }

    *out_value_ptr = static_cast<uint32_t>(value);
    result         = true;

end:
    return result;
}

std::vector<std::string> Stereo::Replay::ConsistencyChecker::CommandLine::split_list(const std::string& in_list)
{
    size_t                   n_first_character = 0;
    std::vector<std::string> result;

    while (n_first_character <= in_list.size() )
    {
        const size_t n_comma = in_list.find(',',
                                            n_first_character);

        result.push_back(in_list.substr(n_first_character,
                                        (n_comma == std::string::npos) ? std::string::npos
                                                                       : n_comma - n_first_character) );

        n_first_character = (n_comma == std::string::npos) ? in_list.size() + 1
                                                           : n_comma + 1;
    }

    return result;
}
