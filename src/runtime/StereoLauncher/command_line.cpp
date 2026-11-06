/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "command_line.h"

const char* Stereo::Launcher::CommandLine::get_usage_ptr()
{
    return "Usage: StereoLauncher --recipe file.json --session id [--arguments \"text\"] [--working-directory dir] [--log] [--wait] application.exe\n"
           "Starts the application with the stereo converter in it. The launcher has to be of the architecture of the application:\n"
           "StereoLauncher_x86.exe for a 32-bit one, StereoLauncher_x64.exe for a 64-bit one.\n"
           "\n"
           "  --recipe file.json        the recipe which converts the application to stereo (required)\n"
           "  --session id              names the playback, so that the programs which take part in it find each other (required)\n"
           "  --arguments \"text\"        put on the command line of the application as it is\n"
           "  --working-directory dir   where the application runs (the current directory by default)\n"
           "  --log                     the converter writes a log of what it does (see SessionEnvironment::get_log_path());\n"
           "                            without it, only why it stops the application is written\n"
           "  --wait                    does not return until the application exits\n";
}

bool Stereo::Launcher::CommandLine::parse(const std::vector<std::string>& in_argument_vec,
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
        const bool         has_next = (n_argument + 1 < in_argument_vec.size() );

        if (argument == "--wait")
        {
            out_options_ptr->should_wait = true;
        }
        else
        if (argument == "--log")
        {
            out_options_ptr->is_logging = true;
        }
        else
        if (argument == "--arguments"         ||
            argument == "--recipe"            ||
            argument == "--session"           ||
            argument == "--working-directory")
        {
            if (has_next == false)
            {
                *out_error_ptr = argument + " needs a value";

                goto end;
            }

            const std::string& value = in_argument_vec.at(++n_argument);

            if (argument == "--arguments")
            {
                out_options_ptr->arguments = value;
            }
            else
            if (argument == "--recipe")
            {
                out_options_ptr->recipe_path = value;
            }
            else
            if (argument == "--session")
            {
                out_options_ptr->session_id = value;
            }
            else
            {
                out_options_ptr->working_directory = value;
            }
        }
        else
        if (argument.empty() == false &&
            argument.at(0)   == '-')
        {
            *out_error_ptr = "Unknown argument " + argument;

            goto end;
        }
        else
        if (out_options_ptr->executable_path.empty() )
        {
            out_options_ptr->executable_path = argument;
        }
        else
        {
            *out_error_ptr = "More than one application is given; the arguments of the application go after --arguments";

            goto end;
        }
    }

    if (out_options_ptr->executable_path.empty() )
    {
        *out_error_ptr = "No application is given";

        goto end;
    }

    if (out_options_ptr->recipe_path.empty() )
    {
        *out_error_ptr = "--recipe is needed";

        goto end;
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
