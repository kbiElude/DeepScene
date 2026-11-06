/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Puts the frames of an analysis in time order: runs of frames of one shape and their speed, how often each shape and pass
 * structure occurs, and setup frames (much longer than a typical frame).
 */
#ifndef APIDUMPANALYSIS_TIMELINE_BUILDER_H
#define APIDUMPANALYSIS_TIMELINE_BUILDER_H

#include "analysis/api_dump_analysis/model.h"

namespace Analysis
{
    namespace APIDumpAnalysis
    {
        class TimelineBuilder
        {
        public:
            static Timeline build(const AnalysisResult& in_analysis);

        private:
            static std::vector<uint64_t> get_setup_frame_index_vec(const Model& in_model);
        };
    }
}

#endif /* APIDUMPANALYSIS_TIMELINE_BUILDER_H */
