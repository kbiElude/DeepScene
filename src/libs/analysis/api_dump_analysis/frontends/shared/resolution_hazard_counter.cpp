/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_analysis/frontends/shared/resolution_hazard_counter.h"

void Analysis::APIDumpAnalysis::Frontends::ResolutionHazardCounter::count(const ResolutionHazard& in_hazard)
{
    m_count_map[in_hazard]++;
}

const std::map<Analysis::APIDumpAnalysis::ResolutionHazard, uint64_t>& Analysis::APIDumpAnalysis::Frontends::ResolutionHazardCounter::get_counts() const
{
    return m_count_map;
}
