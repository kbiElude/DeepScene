/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Stores every access that the model builder reports, and where each frame ends. DependencyAnalyzer reads the log after the whole workload has
 * been processed. An access is small and a draw produces only a few, so keeping all of them is cheap.
 */
#ifndef APIDUMPDEPENDENCIES_ACCESS_LOG_H
#define APIDUMPDEPENDENCIES_ACCESS_LOG_H

#include "analysis/api_dump_analysis/i_access_sink.h"
#include "analysis/api_dump_dependencies/types.h"
#include <stdint.h>
#include <vector>

namespace Analysis
{
    namespace APIDumpDependencies
    {
        class AccessLog : public Analysis::APIDumpAnalysis::IAccessSink
        {
        public:
            AccessLog();

            /* The accesses, in the order they were made. */
            const std::vector<Analysis::APIDumpAnalysis::AccessEvent>& get_event_vec() const;

            /* For each frame that ended, the number of accesses made up to the end of that frame. The accesses of frame N are those from index
             * count[N - 1] (0 for frame 0) up to count[N]. Accesses after the last count belong to a frame that did not end.
             */
            const std::vector<uint64_t>& get_frame_end_event_count_vec() const;

            /* The first and the last call of each frame that ended, as numbered by the model builder. */
            const std::vector<FrameCallRange>& get_frame_call_range_vec() const;

            void on_access     (const Analysis::APIDumpAnalysis::AccessEvent& in_event) override;
            void on_frame_ended(const uint64_t&                               in_frame_index,
                                const uint64_t&                               in_first_call_index,
                                const uint64_t&                               in_last_call_index) override;

        private:
            std::vector<Analysis::APIDumpAnalysis::AccessEvent> m_event_vec;
            std::vector<FrameCallRange>                         m_frame_call_range_vec;
            std::vector<uint64_t>                               m_frame_end_event_count_vec;
        };
    }
}

#endif /* APIDUMPDEPENDENCIES_ACCESS_LOG_H */
