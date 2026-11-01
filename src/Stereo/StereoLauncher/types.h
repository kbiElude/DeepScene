/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREOLAUNCHER_TYPES_H
#define STEREOLAUNCHER_TYPES_H

#include <string>

namespace Stereo
{
    namespace StereoLauncher
    {
        /* What the exit code of the program says. */
        enum class ExitCode
        {
            BAD_ARGUMENTS    = 1,
            CONVERTER_FAILED = 3, /* the converter in the application stopped it, and said why in its log */
            LAUNCH_FAILED    = 2,
            SUCCESS          = 0
        };

        struct Options
        {
            std::string arguments;          /* of the application, as they are put on its command line   */
            std::string executable_path;
            bool        is_logging          = false; /* the converter writes a log of what it does                         */
            std::string recipe_path;
            std::string session_id;
            bool        should_wait         = false; /* until the application exits                       */
            std::string working_directory;  /* empty: that of the launcher                                */
        };
    }
}

#endif /* STEREOLAUNCHER_TYPES_H */
