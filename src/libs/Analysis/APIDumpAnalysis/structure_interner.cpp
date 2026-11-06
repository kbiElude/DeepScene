/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "Analysis/APIDumpAnalysis/structure_interner.h"

template<typename Structure>
Analysis::APIDumpAnalysis::StructureInterner<Structure>::StructureInterner()
{
    /* Stub */
}

template<typename Structure>
const Structure& Analysis::APIDumpAnalysis::StructureInterner<Structure>::get_structure(const uint32_t& in_id) const
{
    return m_structure_vec.at(in_id);
}

template<typename Structure>
size_t Analysis::APIDumpAnalysis::StructureInterner<Structure>::get_n_structures() const
{
    return m_structure_vec.size();
}

template<typename Structure>
uint32_t Analysis::APIDumpAnalysis::StructureInterner<Structure>::intern(const Structure& in_structure,
                                                                         const uint64_t&  in_key,
                                                                         bool*            out_is_new_ptr)
{
    const auto candidate_range = m_id_map.equal_range(in_key);
    bool       is_new          = true;
    uint32_t   result          = 0;

    for (auto candidate_iterator  = candidate_range.first;
              candidate_iterator != candidate_range.second && is_new;
            ++candidate_iterator)
    {
        if (m_structure_vec.at(candidate_iterator->second) == in_structure)
        {
            result = candidate_iterator->second;
            is_new = false;
        }
    }

    if (is_new)
    {
        result = static_cast<uint32_t>(m_structure_vec.size() );

        m_structure_vec.push_back(in_structure);
        m_id_map.insert          ({in_key, result});
    }

    if (out_is_new_ptr != nullptr)
    {
        *out_is_new_ptr = is_new;
    }

    return result;
}

/* The structures which the analysis interns. */
template class Analysis::APIDumpAnalysis::StructureInterner<Analysis::APIDumpAnalysis::FrameShape>;
template class Analysis::APIDumpAnalysis::StructureInterner<Analysis::APIDumpAnalysis::PassStructure>;
