/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_analysis/extractor_progress.h"
#include <stdio.h>

Analysis::APIDumpAnalysis::ExtractorProgress::ExtractorProgress(const double& in_interval_seconds)
    :m_interval_seconds  (in_interval_seconds),
     m_last_progress_time(Clock::now() ),
     m_start_time        (Clock::now() )
{
    /* Stub */
}

double Analysis::APIDumpAnalysis::ExtractorProgress::get_elapsed_seconds() const
{
    return std::chrono::duration<double>(Clock::now() - m_start_time).count();
}

Analysis::APIDumpAnalysis::Frontends::AnalysisSession::ProgressFunc Analysis::APIDumpAnalysis::ExtractorProgress::get_progress_func(const ExtraTextFunc& in_extra_text_func)
{
    return [this, in_extra_text_func](const Frontends::SessionProgress& in_progress)
    {
        const double since_last_seconds = std::chrono::duration<double>(Clock::now() - m_last_progress_time).count();

        if (since_last_seconds >= m_interval_seconds)
        {
            const std::string extra_text = (in_extra_text_func) ? in_extra_text_func(in_progress)
                                                                : std::string();

            m_last_progress_time = Clock::now();

            fprintf(stdout,
                    "[%7.1fs] segments=%llu calls=%llu frames=%llu%s demo=%.1fs\n",
                    get_elapsed_seconds(),
                    static_cast<unsigned long long>(in_progress.n_segments),
                    static_cast<unsigned long long>(in_progress.n_calls),
                    static_cast<unsigned long long>(in_progress.n_frames),
                    extra_text.c_str(),
                    in_progress.demo_seconds);
            fflush(stdout);
        }
    };
}
