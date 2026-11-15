/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_LAUNCHER_TYPES_H
#define STEREO_LAUNCHER_TYPES_H

#include <string>

namespace Stereo
{
    namespace Launcher
    {
        /* The exit codes of the program. */
        enum class ExitCode
        {
            BAD_ARGUMENTS    = 1,
            CONVERTER_FAILED = 3, /* the converter inside the application stopped it and wrote the reason to its log */
            LAUNCH_FAILED    = 2,
            SUCCESS          = 0
        };

        struct Options
        {
            std::string arguments;          /* command line arguments of the application                 */
            std::string click_button_text;  /* text of a button in a window the application shows, which is pressed; empty: none */
            std::string executable_path;
            bool        is_logging          = false; /* the converter writes a log of what it does                         */
            std::string recipe_path;
            std::string session_id;
            bool        should_wait         = false; /* wait until the application exits                  */
            std::string working_directory;  /* empty: the directory of the launcher                       */
        };
    }
}

#endif /* STEREO_LAUNCHER_TYPES_H */
