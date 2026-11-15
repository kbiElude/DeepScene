/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * The progress report of the programs that read a workload. Each time the interval has passed, it prints one line with the segments, calls
 * and frames read so far, the demo time they cover, and any text the program adds.
 */
#ifndef APIDUMPANALYSIS_EXTRACTOR_PROGRESS_H
#define APIDUMPANALYSIS_EXTRACTOR_PROGRESS_H

#include "analysis/api_dump_analysis/frontends/analysis_session.h"
#include <chrono>
#include <functional>
#include <string>

namespace Analysis
{
    namespace APIDumpAnalysis
    {
        class ExtractorProgress
        {
        public:
            /* Returns text to append to the line after the frames, for example " shapes=3" (with the leading space). Empty adds nothing. */
            typedef std::function<std::string(const Frontends::SessionProgress& in_progress)> ExtraTextFunc;

            /* The interval is the time between progress lines, and the clock starts at construction. 0 seconds prints a line for every segment. */
            explicit ExtractorProgress(const double& in_interval_seconds);

            /* Seconds since the object was made. */
            double get_elapsed_seconds() const;

            /* Returns the function to pass to AnalysisSession::run(). It refers to this object, which must outlive the session. */
            Frontends::AnalysisSession::ProgressFunc get_progress_func(const ExtraTextFunc& in_extra_text_func = ExtraTextFunc() );

        private:
            typedef std::chrono::steady_clock Clock;

            double            m_interval_seconds;
            Clock::time_point m_last_progress_time;
            Clock::time_point m_start_time;
        };
    }
}

#endif /* APIDUMPANALYSIS_EXTRACTOR_PROGRESS_H */
