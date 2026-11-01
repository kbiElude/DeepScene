/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "live_object_list.h"
#include <algorithm>
#include <utility>

Stereo::Replay::EngineDLL::LiveObjectList::LiveObjectList()
    :m_n_added(0)
{
    /* Stub */
}

void Stereo::Replay::EngineDLL::LiveObjectList::add(void* in_object_ptr)
{
    const auto order_iterator = m_order_map.lower_bound(in_object_ptr);

    if (order_iterator        == m_order_map.end() ||
        order_iterator->first != in_object_ptr)
    {
        m_order_map.emplace_hint(order_iterator,
                                 in_object_ptr,
                                 m_n_added++);
    }
}

void Stereo::Replay::EngineDLL::LiveObjectList::clear()
{
    m_order_map.clear();
}

std::vector<void*> Stereo::Replay::EngineDLL::LiveObjectList::get_newest_first_vec() const
{
    std::vector<std::pair<uint64_t, void*> > ordered_vec;
    std::vector<void*>                       result_vec;

    ordered_vec.reserve(m_order_map.size() );
    result_vec.reserve (m_order_map.size() );

    for (const auto& order_pair : m_order_map)
    {
        ordered_vec.push_back({order_pair.second,
                               order_pair.first});
    }

    std::sort(ordered_vec.begin(),
              ordered_vec.end  (),
              [](const std::pair<uint64_t, void*>& in_a,
                 const std::pair<uint64_t, void*>& in_b)
              {
                  return (in_a.first > in_b.first);
              });

    for (const auto& ordered_pair : ordered_vec)
    {
        result_vec.push_back(ordered_pair.second);
    }

    return result_vec;
}

void Stereo::Replay::EngineDLL::LiveObjectList::remove(void* in_object_ptr)
{
    m_order_map.erase(in_object_ptr);
}
