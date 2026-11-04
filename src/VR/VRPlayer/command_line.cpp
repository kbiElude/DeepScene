/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "VR/VRPlayer/command_line.h"

const char* VR::VRPlayer::CommandLine::get_usage_ptr()
{
    return "Usage: VRPlayer [--catalog file.json] [--log]\n"
           "Lists the applications of the catalog, which are played in VR with the recipe given for each. Without --catalog, the\n"
           "file apps.json is looked for next to the program, in its apps folder, and in the apps folder above it.\n"
           "With --log the converter and the presenter write a log of each session, to %TEMP%\\apistereo_<session>.log (the newest of them is the last\n"
           "session): what they do each second, and why the headset is or is not used.\n";
}

bool VR::VRPlayer::CommandLine::parse(const std::vector<std::string>& in_argument_vec,
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

        if (argument == "--catalog")
        {
            if (n_argument + 1 >= in_argument_vec.size() )
            {
                *out_error_ptr = "--catalog needs a file";

                goto end;
            }

            out_options_ptr->catalog_filename = in_argument_vec.at(++n_argument);
        }
        else
        if (argument == "--log")
        {
            out_options_ptr->is_logging = true;
        }
        else
        {
            *out_error_ptr = "Unknown argument " + argument;

            goto end;
        }
    }

    result = true;

end:
    return result;
}
