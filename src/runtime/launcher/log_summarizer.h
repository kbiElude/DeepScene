/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * A short report on a run for the person who ran it, built from the log of the converter: how many frames were converted and what the converter
 * could not do (draws with signatures that the recipe does not know, frames of scenes the recipe has no distance for, twins that could not be
 * made, calls that were not made for the right eye). The converter writes its statistics as one log line every so often. The last such line is
 * the summary.
 */
#ifndef STEREO_LAUNCHER_LOG_SUMMARIZER_H
#define STEREO_LAUNCHER_LOG_SUMMARIZER_H

#include "types.h"
#include <string>
#include <vector>

namespace Stereo
{
    namespace Launcher
    {
        class LogSummarizer
        {
        public:
            /* Returns the lines that describe the outcome of the run: the verdict, and one line for each thing the converter could not do. A line starts
             * with "[*] " if all is well and with "[!] " if not. If the log has no statistics, one line says so.
             */
            static std::vector<std::string> make_report_lines(const LogSummary& in_summary);

            /* Reads the last statistics line of the log, if there is one. */
            static LogSummary summarize(const std::vector<std::string>& in_log_line_vec);

        private:
            /* Returns the number that stands right before the text in the line, or 0 if the text is missing or no number precedes it. */
            static uint64_t get_number_before(const std::string& in_line,
                                              const char*        in_text_ptr);
        };
    }
}

#endif /* STEREO_LAUNCHER_LOG_SUMMARIZER_H */
