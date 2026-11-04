/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * What the log of the converter says of a run, in a few lines for the person who ran it: how many frames were converted, and what the converter could not do
 * (draws of signatures that the recipe does not know, frames of scenes it has no distance for, twins that could not be made, calls that were not made for the
 * right eye). The converter writes its statistics as one line every so often; the last of them is the summary.
 */
#ifndef STEREO_LAUNCHER_LOG_SUMMARY_H
#define STEREO_LAUNCHER_LOG_SUMMARY_H

#include <stdint.h>
#include <string>
#include <vector>

namespace Stereo
{
    namespace Launcher
    {
        /* The numbers of the last statistics line of a log; 0 for a number the line does not have (it has some only when Direct3D 7 is converted). */
        struct LogSummary
        {
            bool     has_statistics       = false; /* the log has a line of statistics at all */
            uint64_t n_affected_draws     = 0;
            uint64_t n_failed_twins       = 0;
            uint64_t n_frames             = 0;
            uint64_t n_skipped_draws      = 0;
            uint64_t n_unaffected_draws   = 0;
            uint64_t n_unknown_draws      = 0;
            uint64_t n_unknown_frames     = 0;
            uint64_t n_unmirrored_calls   = 0;
        };

        class LogSummarizer
        {
        public:
            /* The lines that say what the run came to: the verdict, and a line for each thing the converter could not do. Starts with "[*] " for what is
             * well and "[!] " for what is not. If the log has no statistics, one line says so.
             */
            static std::vector<std::string> make_report_lines(const LogSummary& in_summary);

            /* The last line of statistics of the log, if it has one. */
            static LogSummary summarize(const std::vector<std::string>& in_log_line_vec);
        };
    }
}

#endif /* STEREO_LAUNCHER_LOG_SUMMARY_H */
