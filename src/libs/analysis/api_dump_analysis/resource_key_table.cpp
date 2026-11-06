/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_analysis/resource_key_table.h"
#include "analysis/api_dump_analysis/hasher.h"

Analysis::APIDumpAnalysis::ResourceKeyTable::ResourceKeyTable()
{
    /* Stub */
}

void Analysis::APIDumpAnalysis::ResourceKeyTable::begin_frame()
{
    for (const auto& index : m_written_index_vec)
    {
        auto& entry = m_entry_vec.at(index);

        entry.has_writer      = false;
        entry.key             = entry.unwritten_key;
        entry.writer_pass_key = 0;
    }

    m_written_index_vec.clear();
}

uint64_t Analysis::APIDumpAnalysis::ResourceKeyTable::compute_key(const uint64_t& in_description_key,
                                                                  const bool&     in_has_writer,
                                                                  const uint64_t& in_writer_pass_key)
{
    Hasher hasher;

    hasher.add_u64 (in_description_key);
    hasher.add_bool(in_has_writer);
    hasher.add_u64 (in_writer_pass_key);

    return hasher.get_digest();
}

void Analysis::APIDumpAnalysis::ResourceKeyTable::forget_resource(const ResourceId& in_id)
{
    const uint32_t value = get_resource_id_value(in_id);

    if (value != 0 &&
        value <= m_entry_vec.size() )
    {
        m_entry_vec.at(value - 1) = Entry();
    }
}

const Analysis::APIDumpAnalysis::ResourceKeyTable::Entry* Analysis::APIDumpAnalysis::ResourceKeyTable::get_entry_ptr(const ResourceId& in_id) const
{
    const uint32_t value      = get_resource_id_value(in_id);
    const Entry*   result_ptr = nullptr;

    if (value != 0                       &&
        value <= m_entry_vec.size()      &&
        m_entry_vec.at(value - 1).is_registered)
    {
        result_ptr = &m_entry_vec.at(value - 1);
    }

    return result_ptr;
}

uint64_t Analysis::APIDumpAnalysis::ResourceKeyTable::get_key(const ResourceId& in_id) const
{
    const Entry* entry_ptr = get_entry_ptr(in_id);

    return (entry_ptr != nullptr) ? entry_ptr->key
                                  : 0;
}

uint64_t Analysis::APIDumpAnalysis::ResourceKeyTable::get_unwritten_key(const ResourceId& in_id) const
{
    const Entry* entry_ptr = get_entry_ptr(in_id);

    return (entry_ptr != nullptr) ? entry_ptr->unwritten_key
                                  : 0;
}

uint64_t Analysis::APIDumpAnalysis::ResourceKeyTable::get_unwritten_view_key(const ResourceView& in_view) const
{
    Hasher hasher;

    hasher.add_u32(in_view.layer);
    hasher.add_u32(in_view.mip_level);
    hasher.add_u64(get_unwritten_key(in_view.resource_id) );

    return hasher.get_digest();
}

uint64_t Analysis::APIDumpAnalysis::ResourceKeyTable::get_view_key(const ResourceView& in_view) const
{
    Hasher hasher;

    hasher.add_u32(in_view.layer);
    hasher.add_u32(in_view.mip_level);
    hasher.add_u64(get_key(in_view.resource_id) );

    return hasher.get_digest();
}

void Analysis::APIDumpAnalysis::ResourceKeyTable::record_write(const ResourceId& in_id,
                                                               const uint64_t&   in_writer_pass_key)
{
    const uint32_t value = get_resource_id_value(in_id);

    if (get_entry_ptr(in_id) != nullptr)
    {
        auto& entry = m_entry_vec.at(value - 1);

        if (entry.has_writer == false)
        {
            m_written_index_vec.push_back(value - 1);
        }

        entry.has_writer      = true;
        entry.key             = compute_key(entry.description_key,
                                            true, /* in_has_writer */
                                            in_writer_pass_key);
        entry.writer_pass_key = in_writer_pass_key;
    }
}

void Analysis::APIDumpAnalysis::ResourceKeyTable::register_resource(const ResourceId& in_id,
                                                                    const uint64_t&   in_description_key)
{
    const uint32_t value = get_resource_id_value(in_id);

    if (value != 0)
    {
        Entry entry;

        if (value > m_entry_vec.size() )
        {
            m_entry_vec.resize(value);
        }

        entry.description_key = in_description_key;
        entry.is_registered   = true;
        entry.unwritten_key   = compute_key(in_description_key,
                                            false, /* in_has_writer */
                                            0);    /* in_writer_pass_key */
        entry.key             = entry.unwritten_key;

        m_entry_vec.at(value - 1) = entry;
    }
}
