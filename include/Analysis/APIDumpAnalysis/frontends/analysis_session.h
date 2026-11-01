/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Analyzes a workload: reads it a segment at a time, hands each call to the front end of its API, and keeps the model that
 * the front ends build. The calls of an API which has no front end are counted and skipped.
 */
#ifndef APIDUMPANALYSIS_FRONTENDS_ANALYSIS_SESSION_H
#define APIDUMPANALYSIS_FRONTENDS_ANALYSIS_SESSION_H

#include "Analysis/APIDumpAnalysis/frontends/d3d8_frontend.h"
#include "Analysis/APIDumpAnalysis/frontends/types.h"
#include "Analysis/APIDumpAnalysis/model_builder.h"
#include "Analysis/APIDumpAnalysis/types.h"
#include <functional>
#include <string>

namespace Analysis
{
    namespace APIDumpAnalysis
    {
        namespace Frontends
        {
            class AnalysisSession
            {
            public:
                typedef std::function<void(const SessionProgress& in_progress)> ProgressFunc;

                /* Without history no frames are kept; for playing calls instead of analyzing them (see ModelBuilder). */
                explicit AnalysisSession(const bool& in_keeps_history = true);


                /* The frame completed by the last call; valid if process_call returned true. */
                const Analysis::APIDumpAnalysis::CompletedFrame& get_completed_frame() const;

                /* The key of the last draw analyzed by its signature and size (see ModelBuilder::get_last_draw_cover_key). */
                uint64_t get_last_draw_cover_key() const;

                /* The signature of the last draw analyzed (see ModelBuilder::get_last_draw_signature_key). */
                uint64_t get_last_draw_signature_key() const;

                const Analysis::APIDumpAnalysis::Model& get_model() const;

                /* What the workload holds and how it was read. Final once the analysis is over. */
                WorkloadSummary get_summary() const;

                /* The workload read by run(); empty before. */
                WorkloadReference get_workload_reference() const;

                /* True once the limit of set_max_frames() is reached. */
                bool has_reached_frame_limit() const;

                /* Analyzes one call, after the calls before it. The segment holds the call's data chunks. Returns true if the call ended a
                 * frame.
                 */
                bool process_call(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                  const Capture::APIDumpLoader::Workload&      in_segment);

                /* Analyzes the calls of a segment, after those of the segments before it. */
                void process_segment(const Capture::APIDumpLoader::Workload& in_segment);

                /* Analyzes a workload file, reporting progress after each segment (an empty function is no report). Returns false and sets the
                 * error if the file cannot be read. A file that ends abruptly is analyzed as far as it goes; the summary says so.
                 */
                bool run(const char*         in_filename_ptr,
                         const ProgressFunc& in_progress_func,
                         std::string*        out_error_ptr);

                /* Stops the analysis after this many frames; 0 (the default) is no limit. */
                void set_max_frames(const uint64_t& in_max_frames);

                /* The model itself, not a copy: for when the analysis is done. The progress is not to be asked for after it. */
                Analysis::APIDumpAnalysis::Model take_model();

            private:
                static constexpr size_t N_API_GROUPS = static_cast<size_t>(Capture::APIDumpLoader::APIGroup::UNKNOWN) + 1;

                /* The front end of the API, or nullptr if there is none. */
                IAPIFrontend* get_frontend_ptr(const Capture::APIDumpLoader::APIGroup& in_group);

                SessionProgress get_progress() const;

                Analysis::APIDumpAnalysis::ModelBuilder m_builder;
                D3D8Frontend                            m_d3d8_frontend;
                uint64_t                                m_max_frames;
                std::array<uint64_t, N_API_GROUPS>      m_n_calls_by_group_array; /* counted here, and put in the summary when it is asked for */
                uint64_t                                m_n_calls_since_frame;
                WorkloadSummary                         m_summary;
                WorkloadReference                       m_workload_reference;
            };
        }
    }
}

#endif /* APIDUMPANALYSIS_FRONTENDS_ANALYSIS_SESSION_H */
