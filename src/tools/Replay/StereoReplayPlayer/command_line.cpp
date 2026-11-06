/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "command_line.h"
#include "Analysis/APIDumpAnalysis/number_parser.h"

std::vector<std::string> Stereo::Replay::Player::CommandLine::get_handover_argument_vec(const Options& in_options)
{
    std::vector<std::string> result_vec;

    if (in_options.is_console)
    {
        result_vec.push_back("--console");
    }

    if (in_options.recipe_filename.empty() == false)
    {
        result_vec.push_back("--recipe");
        result_vec.push_back(in_options.recipe_filename);
    }

    if (in_options.deps_filename.empty() == false)
    {
        result_vec.push_back("--deps");
        result_vec.push_back(in_options.deps_filename);
    }

    if (in_options.has_eye_mode)
    {
        result_vec.push_back("--eye");
        result_vec.push_back((in_options.eye_mode == EyeMode::LEFT)  ? "left"
                           : (in_options.eye_mode == EyeMode::RIGHT) ? "right"
                                                                     : "both");
    }

    if (in_options.has_eye_separation)
    {
        result_vec.push_back("--eye-separation");
        result_vec.push_back(std::to_string(in_options.eye_separation) );
    }

    if (in_options.has_frame_range)
    {
        result_vec.push_back("--frames");
        result_vec.push_back(std::to_string(in_options.first_frame) + "-" + std::to_string(in_options.last_frame) );
    }

    return result_vec;
}

const char* Stereo::Replay::Player::CommandLine::get_usage_ptr()
{
    return "Usage: StereoReplayPlayer [--console] [--recipe file.json [--eye left|right|both] [--eye-separation D]] [--frames A-B] [file.workload]\n"
           "Without a file, a window asks for one. The window has the play / pause and reset buttons. With --console,\n"
           "the commands are typed instead:\n"
           "p = play / pause, r = reset, s = status, g <frame> = go to a frame (the frames before it are hurried through, and a frame back in time\n"
           "starts the replay over), x <speed> = speed (1 = the recorded pace, 0 = unlimited), q = quit\n"
           "\n"
           "With a recipe, the frames are rendered for the eyes it says (both, by default), and a window shows the images side\n"
           "by side. The recipe changes the matrices of the calls; frames it does not know are treated as it says.\n"
           "\n"
           "The distance between the eyes is D (0.1 if not given) in the units of the camera of the application; the window has a\n"
           "slider for it, and with --console the command e <D> changes it. The recipe says in which direction the eyes move apart.\n"
           "\n"
           "With --frames A-B, the replay runs unpaced up to frame A, plays A to B at the recorded pace, and pauses after B.\n"
           "Frames are counted from 0, as the replay presents them.\n"
           "\n"
           "A seek (the slider, g <frame>) is quicker with the dependencies between the frames of the workload (APIDumpDependencyExtractor makes them): the\n"
           "frames whose content nothing after the frame reads are played without drawing. --deps FILE says where they are; without it, the file which has the\n"
           "name of the workload and the extension .deps is used if it is there.\n";
}

bool Stereo::Replay::Player::CommandLine::parse(const int&   in_argc,
                                                char*        in_argv[],
                                                Options*     out_options_ptr,
                                                std::string* out_error_ptr)
{
    bool result = false;

    *out_options_ptr = Options();

    for (int n_argument = 1;
             n_argument < in_argc;
           ++n_argument)
    {
        const std::string argument = in_argv[n_argument];
        const bool        has_next = (n_argument + 1 < in_argc);

        if (argument == "--console")
        {
            out_options_ptr->is_console = true;
        }
        else
        if (argument == "--deps")
        {
            if (has_next == false)
            {
                *out_error_ptr = "--deps needs a file";

                goto end;
            }

            out_options_ptr->deps_filename = in_argv[++n_argument];
        }
        else
        if (argument == "--recipe")
        {
            if (has_next == false)
            {
                *out_error_ptr = "--recipe needs a file";

                goto end;
            }

            out_options_ptr->recipe_filename = in_argv[++n_argument];
        }
        else
        if (argument == "--eye-separation")
        {
            bool   is_parsed  = false;
            double separation = 0.0;

            if (has_next)
            {
                is_parsed = Analysis::APIDumpAnalysis::NumberParser::parse_double(in_argv[n_argument + 1],
                                                                                 &separation);
            }

            if (is_parsed == false)
            {
                *out_error_ptr = "--eye-separation needs a number which is not negative, such as 0.1";

                goto end;
            }

            out_options_ptr->eye_separation     = static_cast<float>(separation);
            out_options_ptr->has_eye_separation = true;

            ++n_argument;
        }
        else
        if (argument == "--frames")
        {
            bool is_parsed = false;

            if (has_next)
            {
                is_parsed = parse_frame_range(in_argv[n_argument + 1],
                                              out_options_ptr);
            }

            if (is_parsed == false)
            {
                *out_error_ptr = "--frames needs A-B, with B not before A";

                goto end;
            }

            ++n_argument;
        }
        else
        if (argument == "--eye")
        {
            const std::string eye = (has_next) ? in_argv[n_argument + 1]
                                               : "";

            if (eye == "left")
            {
                out_options_ptr->eye_mode = EyeMode::LEFT;
            }
            else
            if (eye == "right")
            {
                out_options_ptr->eye_mode = EyeMode::RIGHT;
            }
            else
            if (eye == "both")
            {
                out_options_ptr->eye_mode = EyeMode::BOTH;
            }
            else
            {
                *out_error_ptr = "--eye needs left, right or both";

                goto end;
            }

            out_options_ptr->has_eye_mode = true;

            ++n_argument;
        }
        else
        if (argument.empty() == false &&
            argument.at(0)   == '-')
        {
            *out_error_ptr = "Unknown option " + argument;

            goto end;
        }
        else
        if (out_options_ptr->workload_path.empty() )
        {
            out_options_ptr->workload_path = argument;
        }
        else
        {
            *out_error_ptr = "More than one workload given";

            goto end;
        }
    }

    if (out_options_ptr->has_eye_mode            &&
        out_options_ptr->recipe_filename.empty() )
    {
        *out_error_ptr = "--eye needs a recipe";

        goto end;
    }

    if (out_options_ptr->has_eye_separation      &&
        out_options_ptr->recipe_filename.empty() )
    {
        *out_error_ptr = "--eye-separation needs a recipe";

        goto end;
    }

    result = true;

end:
    return result;
}

bool Stereo::Replay::Player::CommandLine::parse_frame_range(const char* in_text_ptr,
                                                            Options*    out_options_ptr)
{
    const std::string text   = in_text_ptr;
    const size_t      dash   = text.find('-');
    uint64_t          first  = 0;
    uint64_t          last   = 0;
    bool              result = false;

    if (dash == std::string::npos)
    {
        goto end;
    }

    {
        const std::string first_text = text.substr(0,
                                                   dash);

        if (Analysis::APIDumpAnalysis::NumberParser::parse_unsigned(first_text,
                                                                    UINT32_MAX,
                                                                   &first) == false)
        {
            goto end;
        }
    }

    {
        const std::string last_text = text.substr(dash + 1);

        if (Analysis::APIDumpAnalysis::NumberParser::parse_unsigned(last_text,
                                                                    UINT32_MAX,
                                                                   &last) == false)
        {
            goto end;
        }
    }

    if (last < first)
    {
        goto end;
    }

    out_options_ptr->first_frame     = static_cast<uint32_t>(first);
    out_options_ptr->has_frame_range = true;
    out_options_ptr->last_frame      = static_cast<uint32_t>(last);

    result = true;

end:
    return result;
}
