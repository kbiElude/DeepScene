/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef APIDUMPANALYSIS_FRONTENDS_SHARED_RESOLUTION_HAZARD_COUNTER_H
#define APIDUMPANALYSIS_FRONTENDS_SHARED_RESOLUTION_HAZARD_COUNTER_H

#include "analysis/api_dump_analysis/types.h"
#include <map>
#include <stdint.h>

namespace Analysis
{
    namespace APIDumpAnalysis
    {
        namespace Frontends
        {
            /* How many calls of each kind a front end has seen that depend on the size of what is rendered to in pixels (see ResolutionHazard). */
            class ResolutionHazardCounter
            {
            public:
                void count(const ResolutionHazard& in_hazard);

                /* The calls that were counted, by kind; a kind that was not seen is not in it. */
                const std::map<ResolutionHazard, uint64_t>& get_counts() const;

            private:
                std::map<ResolutionHazard, uint64_t> m_count_map;
            };
        }
    }
}

#endif /* APIDUMPANALYSIS_FRONTENDS_SHARED_RESOLUTION_HAZARD_COUNTER_H */
