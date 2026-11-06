/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "Analysis/APIDumpAnalysis/representative_store.h"

Analysis::APIDumpAnalysis::RepresentativeStore::RepresentativeStore()
{
    /* Stub */
}

void Analysis::APIDumpAnalysis::RepresentativeStore::add(const uint32_t&    in_shape_id,
                                                         const uint64_t&    in_frame_index,
                                                         const uint64_t&    in_weight,
                                                         const FrameDetail& in_detail)
{
    Entry entry;

    entry.detail      = in_detail;
    entry.frame_index = in_frame_index;
    entry.weight      = in_weight;

    const auto slot_iterator = m_slot_map.find(in_shape_id);

    if (slot_iterator == m_slot_map.end() )
    {
        Slot slot;

        slot.first      = entry;
        slot.max_weight = in_weight;
        slot.min_weight = in_weight;

        m_slot_map[in_shape_id] = slot;
    }
    else
    {
        if (in_weight < slot_iterator->second.min_weight)
        {
            slot_iterator->second.has_lightest = true;
            slot_iterator->second.lightest     = entry;
            slot_iterator->second.min_weight   = in_weight;
        }

        if (in_weight > slot_iterator->second.max_weight)
        {
            slot_iterator->second.has_heaviest = true;
            slot_iterator->second.heaviest     = entry;
            slot_iterator->second.max_weight   = in_weight;
        }
    }
}

Analysis::APIDumpAnalysis::RepresentativeStore::EntryPtrVector Analysis::APIDumpAnalysis::RepresentativeStore::get_entry_ptr_vec(const uint32_t& in_shape_id) const
{
    EntryPtrVector result_vec;
    const auto                slot_iterator = m_slot_map.find(in_shape_id);

    if (slot_iterator != m_slot_map.end() )
    {
        result_vec.push_back(&slot_iterator->second.first);

        if (slot_iterator->second.has_lightest)
        {
            result_vec.push_back(&slot_iterator->second.lightest);
        }

        if (slot_iterator->second.has_heaviest)
        {
            result_vec.push_back(&slot_iterator->second.heaviest);
        }
    }

    return result_vec;
}

bool Analysis::APIDumpAnalysis::RepresentativeStore::wants(const uint32_t& in_shape_id,
                                                           const uint64_t& in_weight) const
{
    bool       result        = true;
    const auto slot_iterator = m_slot_map.find(in_shape_id);

    if (slot_iterator != m_slot_map.end() )
    {
        result = (in_weight < slot_iterator->second.min_weight ||
                  in_weight > slot_iterator->second.max_weight);
    }

    return result;
}
