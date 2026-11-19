/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_analysis/draw_key_cache.h"

namespace
{
    /* The draws of a frame come in runs of a few objects with the same material, so a few entries catch most of them. */
    constexpr size_t N_ENTRIES = 4;

    constexpr uint32_t SINGLE_INSTANCE = 1;
}

Analysis::APIDumpAnalysis::DrawKeyCache::DrawKeyCache()
    :m_entry_vec          (N_ENTRIES),
     m_n_last_hit_entry   (0),
     m_n_next_victim_entry(0)
{
    /* Stub */
}

bool Analysis::APIDumpAnalysis::DrawKeyCache::find(const DrawDescription& in_draw,
                                                   const AttachmentSet&   in_attachments,
                                                   const uint64_t&        in_key_table_epoch,
                                                   const bool&            in_is_orthographic,
                                                   DrawKeyResult*         out_result_ptr)
{
    bool result = false;

    /* The entry that matched last is the likeliest to match again. */
    for (size_t n_try = 0;
                n_try < m_entry_vec.size();
              ++n_try)
    {
        const size_t n_entry = (m_n_last_hit_entry + n_try) % m_entry_vec.size();
        const auto&  entry   = m_entry_vec.at(n_entry);

        if (is_match(entry,
                     in_draw,
                     in_attachments,
                     in_key_table_epoch,
                     in_is_orthographic) )
        {
            *out_result_ptr    = entry.result;
            m_n_last_hit_entry = n_entry;
            result             = true;

            break;
        }
    }

    return result;
}

bool Analysis::APIDumpAnalysis::DrawKeyCache::is_match(const Entry&           in_entry,
                                                       const DrawDescription& in_draw,
                                                       const AttachmentSet&   in_attachments,
                                                       const uint64_t&        in_key_table_epoch,
                                                       const bool&            in_is_orthographic)
{
    const bool is_instanced = (in_draw.instance_count > SINGLE_INSTANCE);

    /* The scalars first: they differ between draws more often than the vectors do. */
    return (in_entry.is_used                                                         &&
            in_entry.state_key                      == in_draw.state_key             &&
            in_entry.vertex_layout_key              == in_draw.vertex_layout_key     &&
            in_entry.key_table_epoch                == in_key_table_epoch            &&
            in_entry.is_orthographic                == in_is_orthographic            &&
            in_entry.is_indexed                     == in_draw.is_indexed            &&
            in_entry.is_instanced                   == is_instanced                  &&
            in_entry.primitive_topology             == in_draw.primitive_topology    &&
            in_entry.index_buffer_id                == in_draw.index_buffer_id       &&
            in_entry.shader_id_vec                  == in_draw.shader_id_vec         &&
            in_entry.vertex_buffer_id_vec           == in_draw.vertex_buffer_id_vec  &&
            in_entry.texture_binding_vec            == in_draw.texture_binding_vec   &&
            in_entry.attachments.color_view_vec     == in_attachments.color_view_vec &&
            in_entry.attachments.depth_stencil_view == in_attachments.depth_stencil_view);
}

void Analysis::APIDumpAnalysis::DrawKeyCache::store(const DrawDescription& in_draw,
                                                    const AttachmentSet&   in_attachments,
                                                    const uint64_t&        in_key_table_epoch,
                                                    const bool&            in_is_orthographic,
                                                    const DrawKeyResult&   in_result)
{
    auto& entry = m_entry_vec.at(m_n_next_victim_entry);

    /* Assigned in place, so the vectors of the entry that is forgotten are reused. */
    entry.attachments          = in_attachments;
    entry.index_buffer_id      = in_draw.index_buffer_id;
    entry.is_indexed           = in_draw.is_indexed;
    entry.is_instanced         = (in_draw.instance_count > SINGLE_INSTANCE);
    entry.is_orthographic      = in_is_orthographic;
    entry.is_used              = true;
    entry.key_table_epoch      = in_key_table_epoch;
    entry.primitive_topology   = in_draw.primitive_topology;
    entry.result               = in_result;
    entry.shader_id_vec        = in_draw.shader_id_vec;
    entry.state_key            = in_draw.state_key;
    entry.texture_binding_vec  = in_draw.texture_binding_vec;
    entry.vertex_buffer_id_vec = in_draw.vertex_buffer_id_vec;
    entry.vertex_layout_key    = in_draw.vertex_layout_key;

    m_n_last_hit_entry    = m_n_next_victim_entry;
    m_n_next_victim_entry = (m_n_next_victim_entry + 1) % m_entry_vec.size();
}
