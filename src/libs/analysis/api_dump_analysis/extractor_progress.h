/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * What the programs that read a workload say while they work: a line each time that the interval has gone by, with the segments, the calls and the frames read so
 * far, the demo time they make, and what a program has to add to it.
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
            /* Something to put in the line after the frames, as " shapes=3" (with the space before it); empty for nothing. */
            typedef std::function<std::string(const Frontends::SessionProgress& in_progress)> ExtraTextFunc;

            /* The time is counted from here. 0 seconds is a line for every segment. */
            explicit ExtractorProgress(const double& in_interval_seconds);

            /* Seconds since the object was made. */
            double get_elapsed_seconds() const;

            /* The function for AnalysisSession::run(): it keeps the object, which has to be there while the session runs. */
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
