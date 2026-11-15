/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_analysis/pass_tracker.h"
#include "analysis/api_dump_analysis/key_builder.h"
#include <algorithm>
#include <utility>
#include <vector>

Analysis::APIDumpAnalysis::PassTracker::PassTracker()
    :m_has_open_pass(false)
{
    /* Stub */
}

void Analysis::APIDumpAnalysis::PassTracker::complete(const uint64_t&         in_call_index,
                                                      const bool&             in_keeps_history,
                                                      const ConstantStateMap& in_constant_state_map,
                                                      ResourceKeyTable*       inout_key_table_ptr,
                                                      Model*                  inout_model_ptr,
                                                      FrameInProgress*        inout_frame_ptr)
{
    /* A constant in effect, together with its value. */
    typedef std::vector<uint8_t>                             ValueVector;
    typedef std::pair<ConstantSignature, const ValueVector*> ConstantInEffect;
    typedef std::vector<ConstantInEffect>                    ConstantInEffectVector;

    ConstantInEffectVector constant_vec;
    uint32_t               pass_id        = 0;
    PassStructure          pass_structure;

    pass_structure.clears_color   = m_open_pass.clears_color;
    pass_structure.clears_depth   = m_open_pass.clears_depth;
    pass_structure.clears_stencil = m_open_pass.clears_stencil;
    pass_structure.viewport       = m_open_pass.viewport;

    /* The views are keyed as they were before the pass. A view that the pass clears ignores its old contents. */
    for (const auto& color_view : m_open_pass.attachments.color_view_vec)
    {
        pass_structure.color_view_key_vec.push_back((m_open_pass.clears_color) ? inout_key_table_ptr->get_unwritten_view_key(color_view)
                                                                               : inout_key_table_ptr->get_view_key         (color_view) );
    }

    if (m_open_pass.attachments.depth_stencil_view.resource_id != ResourceId::NONE)
    {
        pass_structure.depth_stencil_view_key = (m_open_pass.clears_depth) ? inout_key_table_ptr->get_unwritten_view_key(m_open_pass.attachments.depth_stencil_view)
                                                                           : inout_key_table_ptr->get_view_key         (m_open_pass.attachments.depth_stencil_view);
    }

    for (const auto& signature_pair : m_open_pass.draw_signature_map)
    {
        pass_structure.draw_signature_vec.push_back    ({signature_pair.second,
                                                         signature_pair.first});
        pass_structure.draw_structure_key_vec.push_back(signature_pair.first);
    }

    for (const auto& state_pair : in_constant_state_map)
    {
        ConstantSignature signature;

        signature.block_key   = state_pair.second.block_key;
        signature.block_kind  = state_pair.second.update.location.block_kind;
        signature.byte_offset = state_pair.second.update.location.byte_offset;
        signature.byte_size   = state_pair.second.update.location.byte_size;
        signature.provenance  = state_pair.second.update.provenance;
        signature.semantic    = state_pair.second.update.semantic;

        constant_vec.push_back({signature,
                               &state_pair.second.update.value_u8_vec});
    }

    const auto is_signature_key_less_func = [](const ConstantInEffect& in_a,
                                               const ConstantInEffect& in_b)
    {
        return KeyBuilder::compute_constant_signature_key(in_a.first) < KeyBuilder::compute_constant_signature_key(in_b.first);
    };

    std::sort(constant_vec.begin(),
              constant_vec.end(),
              is_signature_key_less_func);

    for (const auto& constant : constant_vec)
    {
        pass_structure.constant_signature_vec.push_back(constant.first);
    }

    pass_structure.key = KeyBuilder::compute_pass_structure_key(pass_structure);

    pass_id = inout_model_ptr->pass_structure_interner.intern(pass_structure,
                                                              pass_structure.key);

    /* The camera series, for each kind of pass that has camera constants. */
    for (uint32_t n_constant = 0;
                  n_constant < constant_vec.size();
                ++n_constant)
    {
        const bool is_camera = is_camera_semantic(constant_vec.at(n_constant).first.semantic);

        if (is_camera)
        {
            if (in_keeps_history)
            {
                const ConstantSeriesKey series_key(pass_id,
                                                   n_constant);

                inout_model_ptr->constant_series_map[series_key].add(m_open_pass.begin_timestamp_qpc,
                                                                     *constant_vec.at(n_constant).second);
            }
        }
    }

    /* What the pass rendered to is what later passes read. */
    for (const auto& color_view : m_open_pass.attachments.color_view_vec)
    {
        inout_key_table_ptr->record_write(color_view.resource_id,
                                          pass_structure.key);
    }

    inout_key_table_ptr->record_write(m_open_pass.attachments.depth_stencil_view.resource_id,
                                      pass_structure.key);

    {
        PassInstance instance;

        instance.begin_call_index     = m_open_pass.begin_call_index;
        instance.begin_timestamp_qpc  = m_open_pass.begin_timestamp_qpc;
        instance.end_call_index       = in_call_index;
        instance.last_draw_call_index = m_open_pass.last_draw_call_index;
        instance.n_draws              = m_open_pass.n_draws;
        instance.n_vertices           = m_open_pass.n_vertices;
        instance.structure_id         = pass_id;

        inout_frame_ptr->pass_instance_vec.push_back(instance);

        /* A pass that renders to a presented resource is the pass whose draws the viewer sees as they are. */
        {
            bool is_presented = false;

            for (const auto& color_view : m_open_pass.attachments.color_view_vec)
            {
                const uint32_t id_value = get_resource_id_value(color_view.resource_id);

                if (id_value != 0                                    &&
                    id_value <= inout_model_ptr->resource_vec.size() )
                {
                    is_presented |= (inout_model_ptr->resource_vec.at(id_value - 1).description.kind == ResourceKind::SWAP_CHAIN_IMAGE);
                }
            }

            inout_frame_ptr->pass_is_presented_vec.push_back(is_presented);
        }

        /* The covering keys go straight into the model. The histograms are kept with the frame, because the kind of frame that this pass is in
         * is known only when the frame ends. */
        if (in_keeps_history)
        {
            if (m_open_pass.covering_draw_key_set.empty() == false)
            {
                inout_model_ptr->covering_draw_key_map[pass_id].insert(m_open_pass.covering_draw_key_set.begin(),
                                                                       m_open_pass.covering_draw_key_set.end  () );
            }

            inout_frame_ptr->depth_histogram_vec.push_back        (m_open_pass.depth.depth_histogram);
            inout_frame_ptr->surface_depth_histogram_vec.push_back(m_open_pass.depth.surface_depth_histogram);
            inout_frame_ptr->projection_scale_vec.push_back       (m_open_pass.depth.projection_scale);
        }
    }

    m_has_open_pass = false;
    m_open_pass     = OpenPass();
}

Analysis::APIDumpAnalysis::OpenPass* Analysis::APIDumpAnalysis::PassTracker::get_open_pass_ptr()
{
    return (m_has_open_pass) ? &m_open_pass
                            : nullptr;
}

bool Analysis::APIDumpAnalysis::PassTracker::has_open_pass() const
{
    return m_has_open_pass;
}

bool Analysis::APIDumpAnalysis::PassTracker::is_rendered_to(const ResourceView& in_view) const
{
    bool is_rendered_to = false;

    if (m_has_open_pass                            &&
        in_view.resource_id != ResourceId::NONE)
    {
        is_rendered_to = (in_view.resource_id == m_open_pass.attachments.depth_stencil_view.resource_id);

        for (const auto& color_view : m_open_pass.attachments.color_view_vec)
        {
            is_rendered_to |= (in_view.resource_id == color_view.resource_id);
        }
    }

    return is_rendered_to;
}

void Analysis::APIDumpAnalysis::PassTracker::open_if_needed(const AttachmentSet& in_attachments,
                                                            const uint64_t&      in_call_index,
                                                            const uint64_t&      in_timestamp_qpc,
                                                            const Viewport&      in_viewport)
{
    if (m_has_open_pass == false)
    {
        m_open_pass                     = OpenPass();
        m_open_pass.attachments         = in_attachments;
        m_open_pass.begin_call_index    = in_call_index;
        m_open_pass.begin_timestamp_qpc = in_timestamp_qpc;
        m_open_pass.viewport            = in_viewport;

        m_has_open_pass = true;
    }
}

bool Analysis::APIDumpAnalysis::PassTracker::is_camera_semantic(const ConstantSemantic& in_semantic)
{
    bool result = false;

    switch (in_semantic)
    {
        case ConstantSemantic::PROJECTION_MATRIX:
        case ConstantSemantic::VIEW_MATRIX:
        case ConstantSemantic::VIEW_PROJECTION_MATRIX:
        {
            result = true;

            break;
        }

        default:
        {
            break;
        }
    }

    return result;
}
