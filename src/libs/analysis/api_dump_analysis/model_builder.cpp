/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_analysis/model_builder.h"
#include "analysis/api_dump_analysis/key_builder.h"
#include "utils/hashing/hasher.h"
#include <algorithm>
#include <array>
#include <math.h>
#include <string.h>

Analysis::APIDumpAnalysis::ModelBuilder::ModelBuilder(const bool& in_keeps_history)
    :m_access_sink_ptr          (nullptr),
     m_call_index               (0),
     m_frame_begin_timestamp_qpc(0),
     m_frame_first_call_index   (0),
     m_has_frame_calls          (false),
     m_has_open_pass            (false),
     m_keeps_history            (in_keeps_history),
     m_last_draw_cover_key      (0),
     m_last_draw_signature_key  (0),
     m_n_frames                 (0),
     m_timestamp_qpc            (0)
{
    /* Stub */
}

void Analysis::APIDumpAnalysis::ModelBuilder::begin_call(const uint64_t& in_call_index,
                                                         const uint64_t& in_timestamp_qpc)
{
    m_call_index    = in_call_index;
    m_timestamp_qpc = in_timestamp_qpc;

    if (m_has_frame_calls == false)
    {
        m_frame_begin_timestamp_qpc = in_timestamp_qpc;
        m_frame_first_call_index    = in_call_index;
        m_has_frame_calls           = true;
    }
}

void Analysis::APIDumpAnalysis::ModelBuilder::close_pass()
{
    if (m_has_open_pass)
    {
        complete_pass();
    }
}

void Analysis::APIDumpAnalysis::ModelBuilder::close_pass_if_rendered_to(const ResourceView& in_view)
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

    if (is_rendered_to)
    {
        close_pass();
    }
}

void Analysis::APIDumpAnalysis::ModelBuilder::complete_pass()
{
    /* A constant in effect, with the value that it has. */
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

    /* Views are keyed as before the pass; a view the pass clears ignores its old contents. */
    for (const auto& color_view : m_open_pass.attachments.color_view_vec)
    {
        pass_structure.color_view_key_vec.push_back((m_open_pass.clears_color) ? m_key_table.get_unwritten_view_key(color_view)
                                                                               : m_key_table.get_view_key         (color_view) );
    }

    if (m_open_pass.attachments.depth_stencil_view.resource_id != ResourceId::NONE)
    {
        pass_structure.depth_stencil_view_key = (m_open_pass.clears_depth) ? m_key_table.get_unwritten_view_key(m_open_pass.attachments.depth_stencil_view)
                                                                           : m_key_table.get_view_key         (m_open_pass.attachments.depth_stencil_view);
    }

    for (const auto& signature_pair : m_open_pass.draw_signature_map)
    {
        pass_structure.draw_signature_vec.push_back    ({signature_pair.second,
                                                         signature_pair.first});
        pass_structure.draw_structure_key_vec.push_back(signature_pair.first);
    }

    for (const auto& state_pair : m_constant_state_map)
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

    std::sort(constant_vec.begin(),
              constant_vec.end(),
              [](const ConstantInEffect& in_a,
                 const ConstantInEffect& in_b)
              {
                  return KeyBuilder::compute_constant_signature_key(in_a.first) < KeyBuilder::compute_constant_signature_key(in_b.first);
              });

    for (const auto& constant : constant_vec)
    {
        pass_structure.constant_signature_vec.push_back(constant.first);
    }

    pass_structure.key = KeyBuilder::compute_pass_structure_key(pass_structure);

    pass_id = m_model.pass_structure_interner.intern(pass_structure,
                                                     pass_structure.key);

    /* Camera series, for each kind of pass that has them. */
    for (uint32_t n_constant = 0;
                  n_constant < constant_vec.size();
                ++n_constant)
    {
        const bool is_camera = is_camera_semantic(constant_vec.at(n_constant).first.semantic);

        if (is_camera)
        {
            if (m_keeps_history)
            {
                const ConstantSeriesKey series_key(pass_id,
                                                   n_constant);

                m_model.constant_series_map[series_key].add(m_open_pass.begin_timestamp_qpc,
                                                           *constant_vec.at(n_constant).second);
            }
        }
    }


    /* What the pass has rendered to is what later passes read. */
    for (const auto& color_view : m_open_pass.attachments.color_view_vec)
    {
        m_key_table.record_write(color_view.resource_id,
                                 pass_structure.key);
    }

    m_key_table.record_write(m_open_pass.attachments.depth_stencil_view.resource_id,
                             pass_structure.key);

    {
        PassInstance instance;

        instance.begin_call_index     = m_open_pass.begin_call_index;
        instance.begin_timestamp_qpc  = m_open_pass.begin_timestamp_qpc;
        instance.end_call_index       = m_call_index;
        instance.last_draw_call_index = m_open_pass.last_draw_call_index;
        instance.n_draws              = m_open_pass.n_draws;
        instance.n_vertices           = m_open_pass.n_vertices;
        instance.structure_id         = pass_id;

        m_frame_pass_instance_vec.push_back(instance);

        /* A pass which renders to what is presented is the one whose draws the viewer sees as they are. */
        {
            bool is_presented = false;

            for (const auto& color_view : m_open_pass.attachments.color_view_vec)
            {
                const uint32_t id_value = get_resource_id_value(color_view.resource_id);

                if (id_value != 0                          &&
                    id_value <= m_model.resource_vec.size() )
                {
                    is_presented |= (m_model.resource_vec.at(id_value - 1).description.kind == ResourceKind::SWAP_CHAIN_IMAGE);
                }
            }

            m_frame_pass_is_presented_vec.push_back(is_presented);
        }

        /* Kept with the frame: which kind of frame it is in is known when the frame ends. */
        if (m_keeps_history)
        {
            if (m_open_pass.covering_draw_key_set.empty() == false)
            {
                m_model.covering_draw_key_map[pass_id].insert(m_open_pass.covering_draw_key_set.begin(),
                                                              m_open_pass.covering_draw_key_set.end  () );
            }

            m_frame_depth_histogram_vec.push_back        (m_open_pass.depth_histogram);
            m_frame_surface_depth_histogram_vec.push_back(m_open_pass.surface_depth_histogram);
        }
    }

    m_has_open_pass = false;
    m_open_pass     = OpenPass();
}

uint64_t Analysis::APIDumpAnalysis::ModelBuilder::compute_draw_signature_key(const DrawDescription& in_draw)
{
    std::vector<uint64_t> color_view_key_vec;
    uint64_t              depth_stencil_view_key = 0;

    fill_draw_structure(in_draw,
                        true, /* in_uses_unwritten_keys */
                       &m_draw_signature_scratch);

    for (const auto& color_view : m_open_pass.attachments.color_view_vec)
    {
        color_view_key_vec.push_back(m_key_table.get_unwritten_view_key(color_view) );
    }

    if (m_open_pass.attachments.depth_stencil_view.resource_id != ResourceId::NONE)
    {
        depth_stencil_view_key = m_key_table.get_unwritten_view_key(m_open_pass.attachments.depth_stencil_view);
    }

    const uint64_t structure_key = KeyBuilder::compute_draw_structure_key(m_draw_signature_scratch);

    return KeyBuilder::compute_draw_signature_key(structure_key,
                                                  color_view_key_vec,
                                                  depth_stencil_view_key);
}

void Analysis::APIDumpAnalysis::ModelBuilder::emit_access(const ResourceId&       in_resource_id,
                                                          const AccessRole&       in_role,
                                                          const AccessMode&       in_mode,
                                                          const AccessCoverage&   in_coverage,
                                                          const SubresourceRange& in_subresource_range)
{
    if (m_access_sink_ptr != nullptr &&
        in_resource_id    != ResourceId::NONE)
    {
        AccessEvent event;

        event.call_index        = m_call_index;
        event.coverage          = in_coverage;
        event.mode              = in_mode;
        event.resource_id       = in_resource_id;
        event.role              = in_role;
        event.subresource_range = in_subresource_range;

        m_access_sink_ptr->on_access(event);
    }
}

void Analysis::APIDumpAnalysis::ModelBuilder::fill_draw_structure(const DrawDescription& in_draw,
                                                                  const bool&            in_uses_unwritten_keys,
                                                                  DrawStructure*         out_structure_ptr) const
{
    constexpr uint32_t SINGLE_INSTANCE = 1;

    const auto get_key_func      = [this, in_uses_unwritten_keys](const ResourceId& in_id)
                                   {
                                       return (in_uses_unwritten_keys) ? m_key_table.get_unwritten_key(in_id)
                                                                       : m_key_table.get_key          (in_id);
                                   };
    const auto get_view_key_func = [this, in_uses_unwritten_keys](const ResourceView& in_view)
                                   {
                                       return (in_uses_unwritten_keys) ? m_key_table.get_unwritten_view_key(in_view)
                                                                       : m_key_table.get_view_key          (in_view);
                                   };

    /* The vectors keep what they allocated for the draw before. */
    out_structure_ptr->shader_key_vec.clear();
    out_structure_ptr->texture_binding_key_vec.clear();
    out_structure_ptr->vertex_buffer_key_vec.clear();

    out_structure_ptr->index_buffer_key       = (in_draw.index_buffer_id != ResourceId::NONE) ? get_key_func(in_draw.index_buffer_id)
                                                                                              : 0;
    out_structure_ptr->is_indexed             = in_draw.is_indexed;
    out_structure_ptr->is_instanced           = (in_draw.instance_count > SINGLE_INSTANCE);
    out_structure_ptr->primitive_topology     = in_draw.primitive_topology;
    out_structure_ptr->state_key              = in_draw.state_key;
    out_structure_ptr->vertex_layout_key      = in_draw.vertex_layout_key;

    for (const auto& shader_id : in_draw.shader_id_vec)
    {
        out_structure_ptr->shader_key_vec.push_back(get_key_func(shader_id) );
    }

    for (const auto& binding : in_draw.texture_binding_vec)
    {
        out_structure_ptr->texture_binding_key_vec.push_back({binding.binding_point,
                                                              get_view_key_func(binding.view)});
    }

    for (const auto& vertex_buffer_id : in_draw.vertex_buffer_id_vec)
    {
        out_structure_ptr->vertex_buffer_key_vec.push_back(get_key_func(vertex_buffer_id) );
    }

    /* The order in which the application binds the textures is not what the draw is. */
    std::sort(out_structure_ptr->texture_binding_key_vec.begin(),
              out_structure_ptr->texture_binding_key_vec.end(),
              [](const TextureBindingKey& in_a,
                 const TextureBindingKey& in_b)
              {
                  const auto order_a = std::make_tuple(in_a.binding_point.group,
                                                       in_a.binding_point.index,
                                                       in_a.binding_point.element);
                  const auto order_b = std::make_tuple(in_b.binding_point.group,
                                                       in_b.binding_point.index,
                                                       in_b.binding_point.element);

                  return (order_a < order_b);
              });
}

uint64_t Analysis::APIDumpAnalysis::ModelBuilder::compute_shape_key(const Model&    in_model,
                                                                    const uint32_t& in_shape_id)
{
    std::vector<uint64_t> pass_key_vec;
    const auto&           shape        = in_model.frame_shape_interner.get_structure(in_shape_id);

    for (const auto& pass_id : shape.pass_structure_id_vec)
    {
        pass_key_vec.push_back(in_model.pass_structure_interner.get_structure(pass_id).key);
    }

    return KeyBuilder::compute_key_set_key(pass_key_vec);
}

bool Analysis::APIDumpAnalysis::ModelBuilder::find_pass_index(const CompletedFrame& in_frame,
                                                              const uint64_t&       in_call_index,
                                                              uint32_t*             out_pass_index_ptr)
{
    /* The last draws of the passes only grow, so the first pass at or after the call is found by halving. */
    const auto pass_iterator = std::partition_point(in_frame.pass_instance_vec.begin(),
                                                    in_frame.pass_instance_vec.end  (),
                                                    [&in_call_index](const PassInstance& in_pass)
                                                    {
                                                        return (in_pass.last_draw_call_index < in_call_index);
                                                    });
    const bool result         = (pass_iterator != in_frame.pass_instance_vec.end() );

    if (result)
    {
        *out_pass_index_ptr = static_cast<uint32_t>(pass_iterator - in_frame.pass_instance_vec.begin() );
    }

    return result;
}

bool Analysis::APIDumpAnalysis::ModelBuilder::find_draw_matrices(DrawMatrices* out_matrices_ptr) const
{
    constexpr size_t N_MATRIX_ELEMENTS = 16;
    constexpr size_t PROJECTION_WW     = 15; /* the last element: 1 in an orthographic projection, 0 in a perspective one */

    bool is_orthographic = false;
    bool has_view        = false;
    bool is_model_set    = false;

    *out_matrices_ptr = DrawMatrices();

    /* The lowest model matrix of the constants, in the order they are kept in, is the one that the draws are made with. */
    for (const auto& state_pair : m_constant_state_map)
    {
        const auto& update = state_pair.second.update;

        if (update.value_u8_vec.size() == N_MATRIX_ELEMENTS * sizeof(float) )
        {
            switch (update.semantic)
            {
                case ConstantSemantic::MODEL_MATRIX:
                {
                    if (is_model_set == false)
                    {
                        ::memcpy(out_matrices_ptr->model_f32mat.data(),
                                 update.value_u8_vec.data(),
                                 sizeof(out_matrices_ptr->model_f32mat) );

                        is_model_set = true;
                    }

                    break;
                }

                case ConstantSemantic::PROJECTION_MATRIX:
                {
                    ::memcpy(out_matrices_ptr->projection_f32mat.data(),
                             update.value_u8_vec.data(),
                             sizeof(out_matrices_ptr->projection_f32mat) );

                    out_matrices_ptr->has_projection = true;

                    /* No depth to converge at in a projection which does not shrink what is far. */
                    is_orthographic = (out_matrices_ptr->projection_f32mat.at(PROJECTION_WW) != 0.0f);

                    break;
                }

                case ConstantSemantic::VIEW_MATRIX:
                {
                    ::memcpy(out_matrices_ptr->view_f32mat.data(),
                             update.value_u8_vec.data(),
                             sizeof(out_matrices_ptr->view_f32mat) );

                    has_view = true;

                    break;
                }

                default:
                {
                    break;
                }
            }
        }
    }

    return (has_view        &&
            is_orthographic == false);
}

uint64_t Analysis::APIDumpAnalysis::ModelBuilder::get_last_draw_cover_key() const
{
    return m_last_draw_cover_key;
}

uint64_t Analysis::APIDumpAnalysis::ModelBuilder::get_last_draw_signature_key() const
{
    return m_last_draw_signature_key;
}

Analysis::APIDumpAnalysis::AccessCoverage Analysis::APIDumpAnalysis::ModelBuilder::get_clear_coverage(const ClearOperation& in_clear,
                                                                                                      const ResourceView&   in_view) const
{
    const uint32_t id_value = get_resource_id_value(in_view.resource_id);
    AccessCoverage result   = AccessCoverage::FULL;

    if (in_clear.has_rectangles)
    {
        result = AccessCoverage::PARTIAL;

        goto end;
    }

    if (in_clear.is_limited_to_viewport                 &&
        id_value                        != 0            &&
        id_value                        <= m_model.resource_vec.size() )
    {
        const ResourceDescription& description = m_model.resource_vec.at(id_value - 1).description;
        const uint32_t             height      = std::max<uint32_t>(description.height >> in_view.mip_level,
                                                                    1);
        const uint32_t             width       = std::max<uint32_t>(description.width  >> in_view.mip_level,
                                                                    1);

        if (description.width  == 0 ||
            description.height == 0)
        {
            /* A surface which has no size told is not one to tell the viewport against. */
            result = AccessCoverage::UNKNOWN;
        }
        else
        if (m_viewport.x                     >  0.0f                         ||
            m_viewport.y                     >  0.0f                         ||
            m_viewport.x + m_viewport.width  <  static_cast<float>(width)    ||
            m_viewport.y + m_viewport.height <  static_cast<float>(height) )
        {
            result = AccessCoverage::PARTIAL;
        }
    }

end:
    return result;
}

const Analysis::APIDumpAnalysis::CompletedFrame& Analysis::APIDumpAnalysis::ModelBuilder::get_completed_frame() const
{
    return m_completed_frame;
}

const Analysis::APIDumpAnalysis::Model& Analysis::APIDumpAnalysis::ModelBuilder::get_model() const
{
    return m_model;
}

uint64_t Analysis::APIDumpAnalysis::ModelBuilder::get_n_frames() const
{
    return m_n_frames;
}

const Analysis::APIDumpAnalysis::Viewport& Analysis::APIDumpAnalysis::ModelBuilder::get_viewport() const
{
    return m_viewport;
}

Analysis::APIDumpAnalysis::SubresourceRange Analysis::APIDumpAnalysis::ModelBuilder::get_subresource_range(const ResourceView& in_view)
{
    SubresourceRange result;

    result.first_layer     = in_view.layer;
    result.first_mip_level = in_view.mip_level;
    result.n_layers        = 1;
    result.n_mip_levels    = 1;

    return result;
}

bool Analysis::APIDumpAnalysis::ModelBuilder::is_camera_semantic(const ConstantSemantic& in_semantic)
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

void Analysis::APIDumpAnalysis::ModelBuilder::merge_depth_histograms(const uint32_t&                              in_shape_id,
                                                                     const std::vector<DepthHistogram>&           in_histogram_vec,
                                                                     const std::vector<PassInstance>&             in_pass_instance_vec,
                                                                     std::map<DepthHistogramKey, DepthHistogram>* inout_map_ptr)
{
    for (size_t n_pass = 0;
                n_pass < in_histogram_vec.size();
              ++n_pass)
    {
        if (in_histogram_vec.at(n_pass).is_empty() == false)
        {
            (*inout_map_ptr)[DepthHistogramKey(in_shape_id,
                                               in_pass_instance_vec.at(n_pass).structure_id)].merge(in_histogram_vec.at(n_pass) );
        }
    }
}

void Analysis::APIDumpAnalysis::ModelBuilder::on_clear(const ClearOperation& in_clear)
{
    if (in_clear.clears_color)
    {
        for (const auto& color_view : m_attachments.color_view_vec)
        {
            const AccessCoverage   coverage = get_clear_coverage(in_clear,
                                                                 color_view);
            const SubresourceRange range    = get_subresource_range(color_view);

            m_access_tracker.on_write(color_view.resource_id);

            emit_access(color_view.resource_id,
                        AccessRole::CLEAR_TARGET,
                        AccessMode::WRITE,
                        coverage,
                        range);
        }
    }

    if (in_clear.clears_depth    ||
        in_clear.clears_stencil)
    {
        const AccessCoverage   coverage = get_clear_coverage(in_clear,
                                                             m_attachments.depth_stencil_view);
        const SubresourceRange range    = get_subresource_range(m_attachments.depth_stencil_view);

        m_access_tracker.on_write(m_attachments.depth_stencil_view.resource_id);

        emit_access(m_attachments.depth_stencil_view.resource_id,
                    AccessRole::CLEAR_TARGET,
                    AccessMode::WRITE,
                    coverage,
                    range);
    }

    if (m_has_open_pass         &&
        m_open_pass.has_draws)
    {
        close_pass();
    }

    open_pass_if_needed();

    m_open_pass.last_draw_call_index = m_call_index;

    m_open_pass.clears_color   |= in_clear.clears_color;
    m_open_pass.clears_depth   |= in_clear.clears_depth;
    m_open_pass.clears_stencil |= in_clear.clears_stencil;
}

void Analysis::APIDumpAnalysis::ModelBuilder::on_copy(const ResourceView& in_source,
                                                      const ResourceView& in_destination)
{
    Utils::Hashing::Hasher hasher;
    const SubresourceRange destination_range = get_subresource_range(in_destination);
    const SubresourceRange source_range      = get_subresource_range(in_source);

    m_access_tracker.on_read (in_source.resource_id);
    m_access_tracker.on_write(in_destination.resource_id);

    emit_access(in_source.resource_id,
                AccessRole::TRANSFER_SOURCE,
                AccessMode::READ,
                AccessCoverage::UNKNOWN,
                source_range);
    emit_access(in_destination.resource_id,
                AccessRole::TRANSFER_DESTINATION,
                AccessMode::WRITE,
                AccessCoverage::UNKNOWN, /* which part of it is not told */
                destination_range);

    close_pass_if_rendered_to(in_source);
    close_pass_if_rendered_to(in_destination);

    /* The destination is what the source was. */
    hasher.add_u32(static_cast<uint32_t>   (TransferKind::COPY) );
    hasher.add_u64(m_key_table.get_view_key(in_source) );

    m_key_table.record_write(in_destination.resource_id,
                             hasher.get_digest() );

    {
        TransferRecord record;

        record.call_index  = m_call_index;
        record.destination = in_destination;
        record.kind        = TransferKind::COPY;
        record.source      = in_source;

        m_frame_transfer_record_vec.push_back(record);
    }
}

Analysis::APIDumpAnalysis::ResourceId Analysis::APIDumpAnalysis::ModelBuilder::on_create_resource(const ResourceDescription& in_description,
                                                                                                  const std::string&         in_debug_name,
                                                                                                  const ResourceView&        in_parent_view)
{
    Resource resource;

    resource.created_at_call_index = m_call_index;
    resource.debug_name            = in_debug_name;
    resource.description           = in_description;
    resource.id                    = make_resource_id(static_cast<uint32_t>(m_model.resource_vec.size() ) + 1);
    resource.match_key             = KeyBuilder::compute_description_key(in_description);
    resource.parent_view           = in_parent_view;

    m_model.resource_vec.push_back       (resource);
    m_model.resource_access_vec.push_back(ResourceAccess() );

    m_key_table.register_resource(resource.id,
                                  resource.match_key);

    return resource.id;
}

void Analysis::APIDumpAnalysis::ModelBuilder::on_destroy_resource(const ResourceId& in_id)
{
    const uint32_t index = get_resource_id_value(in_id);

    if (index >= 1                           &&
        index <= m_model.resource_vec.size() )
    {
        m_model.resource_vec.at(index - 1).destroyed_at_call_index = m_call_index;
        m_model.resource_vec.at(index - 1).is_destroyed            = true;

        /* The frame may still refer to it. */
        m_pending_forget_vec.push_back(in_id);
    }
}

void Analysis::APIDumpAnalysis::ModelBuilder::on_draw(const DrawDescription& in_draw)
{
    DrawStructure& structure = m_draw_structure_scratch;

    open_pass_if_needed();
    record_draw_access (in_draw);

    ViewCoverage coverage = ViewCoverage::UNKNOWN;

    if (m_keeps_history)
    {
        if (in_draw.geometry_ptr != nullptr &&
            in_draw.geometry_ptr->is_pre_transformed)
        {
            coverage = record_screen_depth(in_draw);
        }
        else
        {
            record_draw_depth(in_draw);

            coverage = record_draw_surface_depth(in_draw);
        }
    }

    fill_draw_structure(in_draw,
                        false, /* in_uses_unwritten_keys */
                       &structure);

    {
        const uint64_t key            = KeyBuilder::compute_draw_structure_key(structure);
        const auto     known_entry    = m_model.draw_structure_map.find       (key);
        const uint64_t signature_key  = compute_draw_signature_key            (in_draw);
        const auto     signature_pair = m_open_pass.draw_signature_map.emplace(key,
                                                                               signature_key);

        if (known_entry == m_model.draw_structure_map.end() )
        {
            m_model.draw_structure_map.emplace(key,
                                               structure);
        }
        else
        if ((known_entry->second == structure) == false)
        {
            m_model.n_key_collisions++;
        }

        m_last_draw_signature_key = signature_key;

        /* A draw is told by its signature and its size: the draws of one signature (the quads of a flash and of a text, in one dynamic buffer)
         * are not all the same. A key that has covered the view once is kept, even if other draws with it do not: a flash that comes in as a
         * wipe is a quad of one size which is not always the whole picture, and it is not to be moved for the times that it is.
         */
        m_last_draw_cover_key = KeyBuilder::compute_draw_cover_key(signature_key,
                                                                   in_draw.vertex_count,
                                                                   in_draw.primitive_topology);

        if (coverage == ViewCoverage::COVERING)
        {
            m_open_pass.covering_draw_key_set.insert(m_last_draw_cover_key);
        }

        /* The same draw is recognized by one signature in a pass: its targets are the pass's. */
        if (signature_pair.second        == false         &&
            signature_pair.first->second != signature_key)
        {
            m_model.n_key_collisions++;
        }

        /* The pass is not complete yet, so it is the next one to be added to the frame. */
        m_frame_draw_instance_vec.push_back({m_call_index,
                                             m_last_draw_cover_key,
                                             static_cast<uint32_t>(m_frame_pass_instance_vec.size() ),
                                             key});
    }

    m_open_pass.has_draws            = true;
    m_open_pass.last_draw_call_index = m_call_index;

    m_open_pass.n_draws    += 1;
    m_open_pass.n_vertices += in_draw.vertex_count;
}

void Analysis::APIDumpAnalysis::ModelBuilder::on_frame_presented()
{
    FrameDetail           frame_detail;
    FrameRecord           frame_record;
    FrameShape            frame_shape;
    std::vector<uint64_t> pass_key_vec;
    uint64_t              shape_key    = 0;

    close_pass();

    for (const auto& instance : m_frame_pass_instance_vec)
    {
        frame_shape.pass_structure_id_vec.push_back(instance.structure_id);
        pass_key_vec.push_back                     (m_model.pass_structure_interner.get_structure(instance.structure_id).key);

        frame_record.n_draws += instance.n_draws;
    }

    std::sort(frame_shape.pass_structure_id_vec.begin(),
              frame_shape.pass_structure_id_vec.end() );

    frame_record.begin_timestamp_qpc = m_frame_begin_timestamp_qpc;
    frame_record.end_timestamp_qpc   = m_timestamp_qpc;
    frame_record.first_call_index    = m_frame_first_call_index;
    frame_record.frame_index         = m_n_frames;
    frame_record.last_call_index     = m_call_index;
    frame_record.n_passes            = static_cast<uint32_t>              (m_frame_pass_instance_vec.size() );
    shape_key                        = KeyBuilder::compute_key_set_key    (pass_key_vec);
    frame_record.shape_id            = m_model.frame_shape_interner.intern(frame_shape,
                                                                           shape_key);

    merge_depth_histograms(frame_record.shape_id,
                           m_frame_depth_histogram_vec,
                           m_frame_pass_instance_vec,
                          &m_model.depth_histogram_map);
    merge_depth_histograms(frame_record.shape_id,
                           m_frame_surface_depth_histogram_vec,
                           m_frame_pass_instance_vec,
                          &m_model.surface_depth_histogram_map);

    m_completed_frame.first_call_index  = frame_record.first_call_index;
    m_completed_frame.frame_index       = frame_record.frame_index;
    m_completed_frame.last_call_index   = frame_record.last_call_index;
    m_completed_frame.pass_instance_vec = m_frame_pass_instance_vec;
    m_completed_frame.pass_is_presented_vec = m_frame_pass_is_presented_vec;

    /* What was in the completed frame before is cleared with the rest at the end. */
    m_completed_frame.draw_instance_vec.swap(m_frame_draw_instance_vec);

    m_completed_frame.pass_key_vec = pass_key_vec;
    m_completed_frame.shape_key    = shape_key;

    if (m_keeps_history)
    {
        m_model.frame_record_vec.push_back(frame_record);
    }

    m_access_tracker.end_frame(frame_record.frame_index,
                              &m_model.resource_access_vec,
                              &m_completed_frame.loop_carried_resource_id_vec);

    /* Detail only for the frames the store keeps. */
    if (m_keeps_history)
    {
        const bool is_wanted = m_model.representative_store.wants(frame_record.shape_id,
                                                                  frame_record.n_draws);

        if (is_wanted)
        {
            frame_detail.pass_instance_vec   = m_frame_pass_instance_vec;
            frame_detail.transfer_record_vec = m_frame_transfer_record_vec;

            m_model.representative_store.add(frame_record.shape_id,
                                             frame_record.frame_index,
                                             frame_record.n_draws,
                                             frame_detail);
        }
    }

    m_key_table.begin_frame();

    for (const auto& destroyed_id : m_pending_forget_vec)
    {
        m_key_table.forget_resource(destroyed_id);
    }

    m_frame_depth_histogram_vec.clear        ();
    m_frame_draw_instance_vec.clear          ();
    m_frame_pass_instance_vec.clear          ();
    m_frame_pass_is_presented_vec.clear      ();
    m_frame_surface_depth_histogram_vec.clear();
    m_frame_transfer_record_vec.clear();
    m_pending_forget_vec.clear       ();

    m_has_frame_calls = false;

    if (m_access_sink_ptr != nullptr)
    {
        m_access_sink_ptr->on_frame_ended(m_n_frames,
                                          frame_record.first_call_index,
                                          frame_record.last_call_index);
    }

    ++m_n_frames;
}

void Analysis::APIDumpAnalysis::ModelBuilder::on_set_attachments(const AttachmentSet& in_attachments)
{
    close_pass();

    m_attachments = in_attachments;
}

void Analysis::APIDumpAnalysis::ModelBuilder::on_set_viewport(const Viewport& in_viewport)
{
    if (m_has_open_pass                      &&
        m_open_pass.has_draws                &&
        (in_viewport == m_viewport) == false)
    {
        close_pass();
    }

    m_viewport = in_viewport;

    if (m_has_open_pass)
    {
        m_open_pass.viewport = in_viewport;
    }
}

void Analysis::APIDumpAnalysis::ModelBuilder::on_update_constant(const ConstantUpdate& in_update)
{
    if (in_update.semantic != ConstantSemantic::UNKNOWN)
    {
        const uint32_t block_id_value = get_resource_id_value(in_update.location.block_id);
        ConstantState& state          = m_constant_state_map[std::make_tuple(in_update.location.block_kind,
                                                                             block_id_value,
                                                                             in_update.location.byte_offset,
                                                                             in_update.location.byte_size)];

        /* Assigned in place: the value of the update before keeps its memory. */
        state.block_key = m_key_table.get_unwritten_key(in_update.location.block_id);
        state.update    = in_update;
    }
}

void Analysis::APIDumpAnalysis::ModelBuilder::on_upload(const ResourceView&  in_destination,
                                                        const DataReference& in_data)
{
    Utils::Hashing::Hasher hasher;
    const SubresourceRange destination_range = get_subresource_range(in_destination);

    m_access_tracker.on_write(in_destination.resource_id);

    emit_access(in_destination.resource_id,
                AccessRole::UPLOAD_DESTINATION,
                AccessMode::WRITE,
                AccessCoverage::UNKNOWN, /* which part of it is not told */
                destination_range);

    close_pass_if_rendered_to(in_destination);

    /* Contents vary per frame and are not hashed; only the fact of an upload is. */
    hasher.add_u32(static_cast<uint32_t>(TransferKind::UPLOAD) );

    m_key_table.record_write(in_destination.resource_id,
                             hasher.get_digest() );

    {
        TransferRecord record;

        record.call_index  = m_call_index;
        record.data        = in_data;
        record.destination = in_destination;
        record.kind        = TransferKind::UPLOAD;

        m_frame_transfer_record_vec.push_back(record);
    }
}

void Analysis::APIDumpAnalysis::ModelBuilder::set_access_sink(IAccessSink* in_sink_ptr)
{
    m_access_sink_ptr = in_sink_ptr;
}

void Analysis::APIDumpAnalysis::ModelBuilder::open_pass_if_needed()
{
    if (m_has_open_pass == false)
    {
        m_open_pass                     = OpenPass();
        m_open_pass.attachments         = m_attachments;
        m_open_pass.begin_call_index    = m_call_index;
        m_open_pass.begin_timestamp_qpc = m_timestamp_qpc;
        m_open_pass.viewport            = m_viewport;

        m_has_open_pass = true;
    }
}

void Analysis::APIDumpAnalysis::ModelBuilder::record_draw_access(const DrawDescription& in_draw)
{
    for (const auto& binding : in_draw.texture_binding_vec)
    {
        m_access_tracker.on_read(binding.view.resource_id);
    }

    for (const auto& shader_id : in_draw.shader_id_vec)
    {
        m_access_tracker.on_read(shader_id);
    }

    for (const auto& vertex_buffer_id : in_draw.vertex_buffer_id_vec)
    {
        m_access_tracker.on_read(vertex_buffer_id);
    }

    m_access_tracker.on_read(in_draw.index_buffer_id);

    /* Attachments count as read when blending or a depth/stencil test uses their contents. */
    if (in_draw.reads_color_attachments)
    {
        for (const auto& color_view : m_attachments.color_view_vec)
        {
            m_access_tracker.on_read(color_view.resource_id);
        }
    }

    if (in_draw.reads_depth_stencil)
    {
        m_access_tracker.on_read(m_attachments.depth_stencil_view.resource_id);
    }

    for (const auto& color_view : m_attachments.color_view_vec)
    {
        m_access_tracker.on_write(color_view.resource_id);
    }

    m_access_tracker.on_write(m_attachments.depth_stencil_view.resource_id);

    /* What the sink is told: the reads, and then the writes. A texture is read whole (the filter reaches the levels around the one asked for);
     * an attachment is written where the draw reaches, which is not known here.
     */
    for (const auto& binding : in_draw.texture_binding_vec)
    {
        emit_access(binding.view.resource_id,
                    AccessRole::SAMPLED,
                    AccessMode::READ,
                    AccessCoverage::UNKNOWN,
                    SubresourceRange() );
    }

    for (const auto& shader_id : in_draw.shader_id_vec)
    {
        emit_access(shader_id,
                    AccessRole::SHADER_CODE,
                    AccessMode::READ,
                    AccessCoverage::UNKNOWN,
                    SubresourceRange() );
    }

    for (const auto& vertex_buffer_id : in_draw.vertex_buffer_id_vec)
    {
        emit_access(vertex_buffer_id,
                    AccessRole::VERTEX_INPUT,
                    AccessMode::READ,
                    AccessCoverage::UNKNOWN,
                    SubresourceRange() );
    }

    emit_access(in_draw.index_buffer_id,
                AccessRole::INDEX_INPUT,
                AccessMode::READ,
                AccessCoverage::UNKNOWN,
                SubresourceRange() );

    for (const auto& color_view : m_attachments.color_view_vec)
    {
        const SubresourceRange range = get_subresource_range(color_view);

        emit_access(color_view.resource_id,
                    AccessRole::COLOR_ATTACHMENT,
                    (in_draw.reads_color_attachments) ? AccessMode::READ_WRITE
                                                      : AccessMode::WRITE,
                    AccessCoverage::UNKNOWN,
                    range);
    }

    {
        const SubresourceRange range = get_subresource_range(m_attachments.depth_stencil_view);

        emit_access(m_attachments.depth_stencil_view.resource_id,
                    AccessRole::DEPTH_STENCIL_ATTACHMENT,
                    (in_draw.reads_depth_stencil) ? AccessMode::READ_WRITE
                                                  : AccessMode::WRITE,
                    AccessCoverage::UNKNOWN,
                    range);
    }
}

void Analysis::APIDumpAnalysis::ModelBuilder::record_draw_depth(const DrawDescription& in_draw)
{
    constexpr size_t DEPTH_COLUMN = 2;
    constexpr size_t N_COLUMNS    = 4;
    constexpr size_t ORIGIN_ROW   = 3;

    DrawMatrices matrices;
    const bool   has_depth = find_draw_matrices(&matrices);

    if (has_depth)
    {
        const auto& model_f32mat = matrices.model_f32mat;
        const auto& view_f32mat  = matrices.view_f32mat;

        /* The origin of the object, in the space of the camera: the translation of the model matrix, through the view matrix. */
        const float depth = model_f32mat.at(ORIGIN_ROW * N_COLUMNS + 0) * view_f32mat.at(0 * N_COLUMNS + DEPTH_COLUMN) +
                            model_f32mat.at(ORIGIN_ROW * N_COLUMNS + 1) * view_f32mat.at(1 * N_COLUMNS + DEPTH_COLUMN) +
                            model_f32mat.at(ORIGIN_ROW * N_COLUMNS + 2) * view_f32mat.at(2 * N_COLUMNS + DEPTH_COLUMN) +
                            view_f32mat.at (ORIGIN_ROW * N_COLUMNS                                                     + DEPTH_COLUMN);

        m_open_pass.depth_histogram.add(depth,
                                        in_draw.vertex_count);
    }
}

Analysis::APIDumpAnalysis::ViewCoverage Analysis::APIDumpAnalysis::ModelBuilder::record_draw_surface_depth(const DrawDescription& in_draw)
{
    /* How near to an edge of the picture a vertex has to be for the draw to reach it, in the units of the picture (the edges are at 1 and -1). */
    constexpr float EDGE_REACH = 0.995f;

    ViewCoverage result = ViewCoverage::UNKNOWN;

    constexpr size_t N_COLUMNS         = 4;
    constexpr size_t N_VIEW_COLUMNS    = 3; /* x, y and the depth */
    constexpr size_t PROJECTION_SCALE_X = 0;
    constexpr size_t PROJECTION_SCALE_Y = 5;
    constexpr size_t PROJECTION_Z_SCALE = 10;
    constexpr size_t PROJECTION_Z_SHIFT = 14;

    DrawMatrices matrices;

    if (in_draw.geometry_ptr != nullptr)
    {
        if (in_draw.geometry_ptr->position_vec.empty() == false)
        {
            const bool has_depth = find_draw_matrices(&matrices);

            if (has_depth)
            {
                const auto&    position_vec = in_draw.geometry_ptr->position_vec;
                const float*   projection   = matrices.projection_f32mat.data();
                const uint64_t weight       = std::max<uint64_t>(1,
                                                                 in_draw.vertex_count / position_vec.size() );

                /* The nearest depth that is drawn, from the projection: D3D has the near plane at minus the shift over the scale. */
                const float near_depth = (matrices.has_projection && projection[PROJECTION_Z_SCALE] != 0.0f) ? -projection[PROJECTION_Z_SHIFT] / projection[PROJECTION_Z_SCALE]
                                                                                                             : 0.0f;

                /* The model matrix then the view matrix: the first three columns of their product, which is what a vertex needs to be given its
                 * place in the space of the camera.
                 */
                std::array<float, N_COLUMNS * N_VIEW_COLUMNS> model_view_f32mat;

                for (size_t n_row = 0;
                            n_row < N_COLUMNS;
                          ++n_row)
                {
                    for (size_t n_column = 0;
                                n_column < N_VIEW_COLUMNS;
                              ++n_column)
                    {
                        float sum = 0.0f;

                        for (size_t n_term = 0;
                                    n_term < N_COLUMNS;
                                  ++n_term)
                        {
                            sum += matrices.model_f32mat.at(n_row * N_COLUMNS + n_term) * matrices.view_f32mat.at(n_term * N_COLUMNS + n_column);
                        }

                        model_view_f32mat.at(n_row * N_VIEW_COLUMNS + n_column) = sum;
                    }
                }

                float x_max = -1.0e30f;
                float x_min = 1.0e30f;
                float y_max = -1.0e30f;
                float y_min = 1.0e30f;

                for (const auto& position : position_vec)
                {
                    float view_position[N_VIEW_COLUMNS];

                    for (size_t n_column = 0;
                                n_column < N_VIEW_COLUMNS;
                              ++n_column)
                    {
                        view_position[n_column] = position[0] * model_view_f32mat.at(0 * N_VIEW_COLUMNS + n_column) +
                                                  position[1] * model_view_f32mat.at(1 * N_VIEW_COLUMNS + n_column) +
                                                  position[2] * model_view_f32mat.at(2 * N_VIEW_COLUMNS + n_column) +
                                                                model_view_f32mat.at(3 * N_VIEW_COLUMNS + n_column);
                    }

                    const float depth = view_position[2];

                    /* Where the vertex is in the picture, whether it is in view or not: a draw that reaches past the edges covers them. */
                    if (depth > 0.0f && matrices.has_projection)
                    {
                        const float x = view_position[0] * projection[PROJECTION_SCALE_X] / depth;
                        const float y = view_position[1] * projection[PROJECTION_SCALE_Y] / depth;

                        x_max = (x > x_max) ? x : x_max;
                        x_min = (x < x_min) ? x : x_min;
                        y_max = (y > y_max) ? y : y_max;
                        y_min = (y < y_min) ? y : y_min;
                    }

                    /* In front of the camera, and in view: what is behind it or off to the side is not what the eyes look at. */
                    bool is_seen = (depth > 0.0f && depth >= near_depth);

                    if (is_seen && matrices.has_projection)
                    {
                        is_seen = (::fabsf(view_position[0] * projection[PROJECTION_SCALE_X]) <= depth &&
                                   ::fabsf(view_position[1] * projection[PROJECTION_SCALE_Y]) <= depth);
                    }

                    if (is_seen)
                    {
                        m_open_pass.surface_depth_histogram.add(depth,
                                                                weight);
                    }
                }

                /* No point in front of the camera is nothing to say of the view. */
                if (x_min <= x_max)
                {
                    result = (x_min <= -EDGE_REACH &&
                              x_max >=  EDGE_REACH &&
                              y_min <= -EDGE_REACH &&
                              y_max >=  EDGE_REACH) ? ViewCoverage::COVERING
                                                    : ViewCoverage::PARTIAL;
                }
            }
        }
    }

    return result;
}

Analysis::APIDumpAnalysis::ViewCoverage Analysis::APIDumpAnalysis::ModelBuilder::record_screen_depth(const DrawDescription& in_draw)
{
    /* How near to an edge of the viewport a vertex has to be for the draw to reach it, as a part of the size of the viewport. */
    constexpr float EDGE_REACH = 0.005f;

    /* How much the reciprocal of the depth may differ across a draw for it to be flat, as a part of it. */
    constexpr float FLAT_DEPTH_SPREAD = 0.001f;

    ViewCoverage       result               = ViewCoverage::UNKNOWN;
    float              reciprocal_depth_max = 0.0f;
    float              reciprocal_depth_min = 1.0e30f;
    const auto&        position_vec         = in_draw.geometry_ptr->position_vec;
    const uint64_t     weight               = std::max<uint64_t>(1,
                                                                 in_draw.vertex_count / std::max<size_t>(1,
                                                                                                          position_vec.size() ) );
    const bool         has_viewport         = (m_viewport.width > 0.0f && m_viewport.height > 0.0f);
    std::vector<float> seen_depth_vec;
    float              x_max                = -1.0e30f;
    float              x_min                = 1.0e30f;
    float              y_max                = -1.0e30f;
    float              y_min                = 1.0e30f;

    for (const auto& position : position_vec)
    {
        const float reciprocal_depth = position[2];

        /* A vertex that is not in front of the camera has no depth to tell, and is no part of the view. */
        if (reciprocal_depth > 0.0f)
        {
            const float depth      = 1.0f / reciprocal_depth;
            const bool  is_in_view = (has_viewport == false                                                          ||
                                      (position[0] >= m_viewport.x                                                  &&
                                       position[0] <= m_viewport.x + m_viewport.width                               &&
                                       position[1] >= m_viewport.y                                                  &&
                                       position[1] <= m_viewport.y + m_viewport.height) );

            x_max = (position[0] > x_max) ? position[0] : x_max;
            x_min = (position[0] < x_min) ? position[0] : x_min;
            y_max = (position[1] > y_max) ? position[1] : y_max;
            y_min = (position[1] < y_min) ? position[1] : y_min;

            reciprocal_depth_max = (reciprocal_depth > reciprocal_depth_max) ? reciprocal_depth : reciprocal_depth_max;
            reciprocal_depth_min = (reciprocal_depth < reciprocal_depth_min) ? reciprocal_depth : reciprocal_depth_min;

            if (is_in_view)
            {
                m_open_pass.surface_depth_histogram.add(depth,
                                                        weight);

                seen_depth_vec.push_back(depth);
            }
        }
    }

    /* The depth of the draw, as the matrices tell it for the other draws (the origin of the object): the middle of what was seen. */
    if (seen_depth_vec.empty() == false)
    {
        std::sort(seen_depth_vec.begin(),
                  seen_depth_vec.end() );

        m_open_pass.depth_histogram.add(seen_depth_vec.at(seen_depth_vec.size() / 2),
                                        in_draw.vertex_count);
    }

    if (x_min <= x_max &&
        has_viewport)
    {
        const bool reaches_sides = (x_min <= m_viewport.x                    + EDGE_REACH * m_viewport.width &&
                                    x_max >= m_viewport.x + m_viewport.width - EDGE_REACH * m_viewport.width);
        const bool reaches_ends  = (y_min <= m_viewport.y                     + EDGE_REACH * m_viewport.height &&
                                    y_max >= m_viewport.y + m_viewport.height - EDGE_REACH * m_viewport.height);

        /* What matters to the eyes is the sides: moved sideways, a picture which reaches both leaves a strip bare at one of them. A flat quad (all of
         * it at one depth, as a backdrop, a flash and a fade are) that goes from one side to the other covers the picture, whether the picture
         * has bars above and below it or not; a mesh that goes from one side to the other (a floor) is not one that covers it.
         */
        const bool is_flat        = (reciprocal_depth_max - reciprocal_depth_min <= FLAT_DEPTH_SPREAD * reciprocal_depth_max);

        result = (reaches_sides                       &&
                  (reaches_ends || is_flat) ) ? ViewCoverage::COVERING
                                              : ViewCoverage::PARTIAL;
    }

    return result;
}

bool Analysis::APIDumpAnalysis::ModelBuilder::wants_geometry() const
{
    /* Each frame, today: a sample is at most a few dozen vertices, and the front end keeps the ones it has made. */
    constexpr uint64_t FRAME_STRIDE = 1;

    return (m_keeps_history &&
            m_n_frames % FRAME_STRIDE == 0);
}

Analysis::APIDumpAnalysis::Model Analysis::APIDumpAnalysis::ModelBuilder::take_model()
{
    return std::move(m_model);
}
