/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_LAUNCHER_TYPES_H
#define STEREO_LAUNCHER_TYPES_H

#include <stdint.h>
#include <string>

namespace Stereo
{
    namespace Launcher
    {
        /* What became of the checkbox that is to be unticked before the button is pressed (see ButtonClicker). */
        enum class CheckboxOutcome
        {
            ALREADY_UNTICKED, /* it was not ticked                                */
            NOT_ASKED,        /* no checkbox was named                            */
            NOT_FOUND,        /* the window has no checkbox of that text          */
            STILL_TICKED,     /* it was clicked, and it is ticked all the same    */
            UNTICKED          /* it was ticked, and clicking it unticked it       */
        };

        /* The exit codes of the program. */
        enum class ExitCode
        {
            BAD_ARGUMENTS    = 1,
            CONVERTER_FAILED = 3, /* the converter inside the application stopped it and wrote the reason to its log */
            LAUNCH_FAILED    = 2,
            SUCCESS          = 0
        };

        /* The numbers from the last statistics line of a log. A number is 0 if the line does not have it (some numbers appear only when Direct3D 7 is converted). */
        struct LogSummary
        {
            bool     has_statistics       = false; /* the log has at least one statistics line */
            uint64_t n_affected_draws     = 0;
            uint64_t n_failed_twins       = 0;
            uint64_t n_frames             = 0;
            uint64_t n_skipped_draws      = 0;
            uint64_t n_unaffected_draws   = 0;
            uint64_t n_unknown_draws      = 0;
            uint64_t n_unknown_frames     = 0;
            uint64_t n_unmirrored_calls   = 0;
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
            std::string untick_checkbox_text; /* text of a checkbox in the same window, which is unticked before the button is pressed; empty: none */
            std::string working_directory;  /* empty: the directory of the launcher                       */
        };
    }
}

#endif /* STEREO_LAUNCHER_TYPES_H */
