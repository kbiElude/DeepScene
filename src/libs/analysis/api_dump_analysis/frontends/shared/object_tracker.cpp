/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_analysis/frontends/shared/object_tracker.h"

Analysis::APIDumpAnalysis::Frontends::ObjectTracker::ObjectTracker()
{
    /* Stub */
}

void Analysis::APIDumpAnalysis::Frontends::ObjectTracker::forget(const uint64_t& in_address)
{
    m_object_map.erase(in_address);
}

size_t Analysis::APIDumpAnalysis::Frontends::ObjectTracker::get_n_objects() const
{
    return m_object_map.size();
}

const Analysis::APIDumpAnalysis::Frontends::ObjectTracker::TrackedObject* Analysis::APIDumpAnalysis::Frontends::ObjectTracker::get_object_ptr(const uint64_t& in_address) const
{
    const auto           object_iterator = m_object_map.find(in_address);
    const TrackedObject* result_ptr      = nullptr;

    if (object_iterator != m_object_map.end() )
    {
        result_ptr = &object_iterator->second;
    }

    return result_ptr;
}

void Analysis::APIDumpAnalysis::Frontends::ObjectTracker::track(const uint64_t&      in_address,
                                                                const TrackedObject& in_object)
{
    m_object_map[in_address] = in_object;
}
