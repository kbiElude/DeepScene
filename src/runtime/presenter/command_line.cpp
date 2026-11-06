/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "command_line.h"

const char* Stereo::Presenter::CommandLine::get_usage_ptr()
{
    return "Usage: StereoPresenter --session id [--title text] [--headset [--no-pacing]] [--log]\n"
           "Shows the images which the stereo converter of an application passes on. The session is the one which the converter\n"
           "was started with (see StereoLauncher); VRPlayer starts this program together with the launcher. With --headset the images\n"
           "are shown in the headset of the OpenXR runtime as well, if it has one; the application is then made to produce a frame for\n"
           "each frame of the headset, unless --no-pacing is given. With --log what the presenter does is written to the log of the session.\n";
}

bool Stereo::Presenter::CommandLine::parse(const std::vector<std::string>& in_argument_vec,
                                           Options*                        out_options_ptr,
                                           std::string*                    out_error_ptr)
{
    bool result = false;

    *out_options_ptr = Options();

    for (size_t n_argument = 0;
                n_argument < in_argument_vec.size();
              ++n_argument)
    {
        const std::string& argument = in_argument_vec.at(n_argument);

        if (argument == "--headset")
        {
            out_options_ptr->use_headset = true;
        }
        else
        if (argument == "--no-pacing")
        {
            out_options_ptr->use_pacing = false;
        }
        else
        if (argument == "--log")
        {
            out_options_ptr->use_log = true;
        }
        else
        if (argument == "--session" ||
            argument == "--title")
        {
            if (n_argument + 1 >= in_argument_vec.size() )
            {
                *out_error_ptr = argument + " needs a value";

                goto end;
            }

            ((argument == "--session") ? out_options_ptr->session_id
                                       : out_options_ptr->title) = in_argument_vec.at(++n_argument);
        }
        else
        {
            *out_error_ptr = "Unknown argument " + argument;

            goto end;
        }
    }

    if (out_options_ptr->session_id.empty() )
    {
        *out_error_ptr = "--session is needed";

        goto end;
    }

    result = true;

end:
    return result;
}
