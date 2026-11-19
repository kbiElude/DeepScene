/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_analysis/model_builder.h"
#include "analysis/api_dump_analysis/draw_measurer.h"
#include "analysis/api_dump_analysis/match_key_builder.h"
#include "utils/hashing/stable_hasher.h"
#include <algorithm>
#include <array>
#include <math.h>
#include <string.h>

Analysis::APIDumpAnalysis::ModelBuilder::ModelBuilder(const bool& in_keeps_history)
    :m_access_sink_ptr          (nullptr),
     m_caches_draw_keys         (true),
     m_call_index               (0),
     m_constant_state_epoch     (1),
     m_is_orthographic          (false),
     m_keeps_history            (in_keeps_history),
     m_last_draw_cover_key      (0),
     m_last_draw_signature_key  (0),
     m_n_frames                 (0),
     m_orthographic_epoch       (0),
     m_texture_sink_ptr         (nullptr),
     m_timestamp_qpc            (0)
{
    /* Stub */
}

void Analysis::APIDumpAnalysis::ModelBuilder::begin_call(const uint64_t& in_call_index,
                                                         const uint64_t& in_timestamp_qpc)
{
    m_call_index    = in_call_index;
    m_timestamp_qpc = in_timestamp_qpc;

    m_call_draw_key_vec.clear();

    if (m_frame.has_calls == false)
    {
        m_frame.begin_timestamp_qpc = in_timestamp_qpc;
        m_frame.first_call_index    = in_call_index;
        m_frame.has_calls           = true;
    }
}

void Analysis::APIDumpAnalysis::ModelBuilder::close_pass()
{
    if (m_pass_tracker.has_open_pass() )
    {
        m_pass_tracker.complete(m_call_index,
                                m_keeps_history,
                                m_constant_state_map,
                               &m_key_table,
                               &m_model,
                               &m_frame);
    }
}

void Analysis::APIDumpAnalysis::ModelBuilder::close_pass_if_rendered_to(const ResourceView& in_view)
{
    const bool is_rendered_to = m_pass_tracker.is_rendered_to(in_view);

    if (is_rendered_to)
    {
        close_pass();
    }
}

uint64_t Analysis::APIDumpAnalysis::ModelBuilder::compute_draw_signature_key(const DrawDescription& in_draw,
                                                                             const bool&            in_is_orthographic)
{
    std::vector<uint64_t> color_view_key_vec;
    uint64_t              depth_stencil_view_key = 0;
    const OpenPass*       open_pass_ptr          = m_pass_tracker.get_open_pass_ptr();

    fill_draw_structure(in_draw,
                        true, /* in_uses_unwritten_keys */
                        in_is_orthographic,
                       &m_draw_signature_scratch);

    for (const auto& color_view : open_pass_ptr->attachments.color_view_vec)
    {
        color_view_key_vec.push_back(m_key_table.get_unwritten_view_key(color_view) );
    }

    if (open_pass_ptr->attachments.depth_stencil_view.resource_id != ResourceId::NONE)
    {
        depth_stencil_view_key = m_key_table.get_unwritten_view_key(open_pass_ptr->attachments.depth_stencil_view);
    }

    const uint64_t structure_key = MatchKeyBuilder::compute_draw_structure_key(m_draw_signature_scratch);

    return MatchKeyBuilder::compute_draw_signature_key(structure_key,
                                                       color_view_key_vec,
                                                       depth_stencil_view_key);
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

    return MatchKeyBuilder::compute_key_set_key(pass_key_vec);
}

void Analysis::APIDumpAnalysis::ModelBuilder::emit_access(const ResourceId&       in_resource_id,
                                                          const AccessRole&       in_role,
                                                          const AccessMode&       in_mode,
                                                          const AccessCoverage&   in_coverage,
                                                          const SubresourceRange& in_subresource_range,
                                                          const AccessRectangle&  in_rectangle)
{
    if (m_access_sink_ptr != nullptr &&
        in_resource_id    != ResourceId::NONE)
    {
        AccessEvent event;

        event.call_index        = m_call_index;
        event.coverage          = in_coverage;
        event.mode              = in_mode;
        event.rectangle         = in_rectangle;
        event.resource_id       = in_resource_id;
        event.role              = in_role;
        event.subresource_range = in_subresource_range;

        m_access_sink_ptr->on_access(event);
    }
}

void Analysis::APIDumpAnalysis::ModelBuilder::fill_draw_structure(const DrawDescription& in_draw,
                                                                  const bool&            in_uses_unwritten_keys,
                                                                  const bool&            in_is_orthographic,
                                                                  DrawStructure*         out_structure_ptr) const
{
    constexpr uint32_t SINGLE_INSTANCE = 1;

    /* A draw with an orthographic projection (where distance does not make things smaller) is a different kind of draw than one with a
     * perspective projection, because the same state is used by the panels of a screen and by the scene.
     */
    const bool is_orthographic = in_is_orthographic;

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

    /* The vectors keep the memory that they allocated for the previous draw. */
    out_structure_ptr->shader_key_vec.clear();
    out_structure_ptr->texture_binding_key_vec.clear();
    out_structure_ptr->vertex_buffer_key_vec.clear();

    out_structure_ptr->index_buffer_key   = (in_draw.index_buffer_id != ResourceId::NONE) ? get_key_func(in_draw.index_buffer_id)
                                                                                          : 0;
    out_structure_ptr->is_indexed         = in_draw.is_indexed;
    out_structure_ptr->is_instanced       = (in_draw.instance_count > SINGLE_INSTANCE);
    out_structure_ptr->is_orthographic    = is_orthographic;
    out_structure_ptr->primitive_topology = in_draw.primitive_topology;
    out_structure_ptr->state_key          = in_draw.state_key;
    out_structure_ptr->vertex_layout_key  = in_draw.vertex_layout_key;

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

    const auto is_binding_less_func = [](const TextureBindingKey& in_a,
                                         const TextureBindingKey& in_b)
    {
        const auto order_a = std::make_tuple(in_a.binding_point.group,
                                             in_a.binding_point.index,
                                             in_a.binding_point.element);
        const auto order_b = std::make_tuple(in_b.binding_point.group,
                                             in_b.binding_point.index,
                                             in_b.binding_point.element);

        return (order_a < order_b);
    };

    /* The order in which the application binds the textures does not change what the draw is. */
    std::sort(out_structure_ptr->texture_binding_key_vec.begin(),
              out_structure_ptr->texture_binding_key_vec.end(),
              is_binding_less_func);
}

bool Analysis::APIDumpAnalysis::ModelBuilder::find_pass_index(const CompletedFrame& in_frame,
                                                              const uint64_t&       in_call_index,
                                                              uint32_t*             out_pass_index_ptr)
{
    const auto is_pass_before_call_func = [&in_call_index](const PassInstance& in_pass)
    {
        return (in_pass.last_draw_call_index < in_call_index);
    };

    /* The last draws of the passes only grow, so the first pass at or after the call is found by halving. */
    const auto pass_iterator = std::partition_point(in_frame.pass_instance_vec.begin(),
                                                    in_frame.pass_instance_vec.end  (),
                                                    is_pass_before_call_func);
    const bool result         = (pass_iterator != in_frame.pass_instance_vec.end() );

    if (result)
    {
        *out_pass_index_ptr = static_cast<uint32_t>(pass_iterator - in_frame.pass_instance_vec.begin() );
    }

    return result;
}

const std::vector<Analysis::APIDumpAnalysis::DrawKeys>& Analysis::APIDumpAnalysis::ModelBuilder::get_call_draw_key_vec() const
{
    return m_call_draw_key_vec;
}

uint64_t Analysis::APIDumpAnalysis::ModelBuilder::get_call_index() const
{
    return m_call_index;
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

    if (in_clear.is_limited_to_viewport                                &&
        id_value                        != 0                           &&
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
            /* A surface with no known size cannot be compared with the viewport. */
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

bool Analysis::APIDumpAnalysis::ModelBuilder::get_is_orthographic()
{
    /* The constants only change in on_update_constant(), which counts the updates, so the answer is found again only after one. */
    if (m_orthographic_epoch != m_constant_state_epoch)
    {
        DrawMatrices matrices;

        DrawMeasurer::find_draw_matrices(m_constant_state_map,
                                        &matrices);

        m_is_orthographic    = matrices.is_orthographic;
        m_orthographic_epoch = m_constant_state_epoch;
    }

    return m_is_orthographic;
}

uint64_t Analysis::APIDumpAnalysis::ModelBuilder::get_last_draw_cover_key() const
{
    return m_last_draw_cover_key;
}

uint64_t Analysis::APIDumpAnalysis::ModelBuilder::get_last_draw_signature_key() const
{
    return m_last_draw_signature_key;
}

const Analysis::APIDumpAnalysis::Model& Analysis::APIDumpAnalysis::ModelBuilder::get_model() const
{
    return m_model;
}

uint64_t Analysis::APIDumpAnalysis::ModelBuilder::get_n_frames() const
{
    return m_n_frames;
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

const Analysis::APIDumpAnalysis::Viewport& Analysis::APIDumpAnalysis::ModelBuilder::get_viewport() const
{
    return m_viewport;
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
            const AccessCoverage   coverage = get_clear_coverage   (in_clear,
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
        const AccessCoverage   coverage = get_clear_coverage   (in_clear,
                                                                m_attachments.depth_stencil_view);
        const SubresourceRange range    = get_subresource_range(m_attachments.depth_stencil_view);

        m_access_tracker.on_write(m_attachments.depth_stencil_view.resource_id);

        emit_access(m_attachments.depth_stencil_view.resource_id,
                    AccessRole::CLEAR_TARGET,
                    AccessMode::WRITE,
                    coverage,
                    range);
    }

    OpenPass* open_pass_ptr = m_pass_tracker.get_open_pass_ptr();

    if (open_pass_ptr != nullptr &&
        open_pass_ptr->has_draws)
    {
        close_pass();
    }

    open_pass_if_needed();

    open_pass_ptr = m_pass_tracker.get_open_pass_ptr();

    open_pass_ptr->last_draw_call_index = m_call_index;

    open_pass_ptr->clears_color   |= in_clear.clears_color;
    open_pass_ptr->clears_depth   |= in_clear.clears_depth;
    open_pass_ptr->clears_stencil |= in_clear.clears_stencil;
}

void Analysis::APIDumpAnalysis::ModelBuilder::on_copy(const ResourceView&    in_source,
                                                      const ResourceView&    in_destination,
                                                      const AccessRectangle& in_destination_rectangle)
{
    Utils::Hashing::StableHasher hasher;
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
                (in_destination_rectangle.is_known) ? AccessCoverage::PARTIAL
                                                    : AccessCoverage::UNKNOWN, /* which part is written is unknown unless the front end says so */
                destination_range,
                in_destination_rectangle);

    close_pass_if_rendered_to(in_source);
    close_pass_if_rendered_to(in_destination);

    /* A draw that samples the destination of a copy of the picture reads what is already rendered below it, and the content of the
     * destination is produced separately for each eye. */
    for (const auto& color_view : m_attachments.color_view_vec)
    {
        if (color_view.resource_id == in_source.resource_id)
        {
            m_color_copy_target_set.insert(in_destination.resource_id);

            break;
        }
    }

    /* The destination gets the same content as the source, so it gets the key of the source. */
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

        m_frame.transfer_record_vec.push_back(record);
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
    resource.match_key             = MatchKeyBuilder::compute_description_key(in_description);
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

        /* The frame being built may still refer to the resource, so it is forgotten later. */
        m_pending_forget_vec.push_back(in_id);
    }
}

void Analysis::APIDumpAnalysis::ModelBuilder::on_draw(const DrawDescription& in_draw)
{
    DrawStructure& structure = m_draw_structure_scratch;

    open_pass_if_needed();
    record_draw_access (in_draw);

    OpenPass* open_pass_ptr = m_pass_tracker.get_open_pass_ptr();

    ViewCoverage coverage = ViewCoverage::UNKNOWN;

    if (m_keeps_history)
    {
        if (in_draw.geometry_ptr != nullptr &&
            in_draw.geometry_ptr->is_pre_transformed)
        {
            coverage = DrawMeasurer::record_screen_depth(in_draw,
                                                         m_viewport,
                                                        &open_pass_ptr->depth);
        }
        else
        {
            DrawMeasurer::record_draw_depth(in_draw,
                                            m_constant_state_map,
                                           &open_pass_ptr->depth);

            coverage = DrawMeasurer::record_draw_surface_depth(in_draw,
                                                               m_constant_state_map,
                                                              &open_pass_ptr->depth);
        }
    }

    {
        const bool     is_orthographic = get_is_orthographic();
        DrawKeyResult  keys;
        const uint64_t key_table_epoch = m_key_table.get_epoch();

        /* The keys are a function of the draw, the render targets, the keys of the resources and the projection kind, so a draw that repeats an earlier one has its keys. */
        const bool is_cached = (m_caches_draw_keys                                 &&
                                m_draw_key_cache.find(in_draw,
                                                      open_pass_ptr->attachments,
                                                      key_table_epoch,
                                                      is_orthographic,
                                                     &keys) );

        if (is_cached == false)
        {
            fill_draw_structure(in_draw,
                                false, /* in_uses_unwritten_keys */
                                is_orthographic,
                               &structure);

            keys.structure_key = MatchKeyBuilder::compute_draw_structure_key(structure);
            keys.signature_key = compute_draw_signature_key                 (in_draw,
                                                                             is_orthographic);

            const auto known_entry = m_model.draw_structure_map.find(keys.structure_key);

            if (known_entry == m_model.draw_structure_map.end() )
            {
                m_model.draw_structure_map.emplace(keys.structure_key,
                                                   structure);
            }
            else
            {
                keys.is_collision = ((known_entry->second == structure) == false);
            }

            if (m_caches_draw_keys)
            {
                m_draw_key_cache.store(in_draw,
                                       open_pass_ptr->attachments,
                                       key_table_epoch,
                                       is_orthographic,
                                       keys);
            }
        }

        if (keys.is_collision)
        {
            m_model.n_key_collisions++;
        }

        const uint64_t key            = keys.structure_key;
        const uint64_t signature_key  = keys.signature_key;
        const auto     signature_pair = open_pass_ptr->draw_signature_map.emplace(key,
                                                                                  signature_key);

        m_last_draw_signature_key = signature_key;

        /* A draw is identified by signature and size, as draws of one signature differ (quads sharing a dynamic buffer). A key that
         * covered the view once stays covering, even if other draws with that key do not.
         */
        m_last_draw_cover_key = MatchKeyBuilder::compute_draw_cover_key(signature_key,
                                                                        in_draw.vertex_count,
                                                                        in_draw.primitive_topology);

        const DrawFactsKey draw_facts_key(key,
                                          m_last_draw_cover_key);

        DrawFacts& draw_facts = m_model.draw_facts_map[draw_facts_key];

        DrawMeasurer::record_draw_facts(in_draw,
                                        m_color_copy_target_set,
                                        m_keeps_history,
                                        m_constant_state_map,
                                        m_viewport,
                                       &m_frame.has_seen_3d_draw,
                                       &draw_facts);

        if (coverage == ViewCoverage::COVERING)
        {
            open_pass_ptr->covering_draw_key_set.insert(m_last_draw_cover_key);
        }

        m_call_draw_key_vec.push_back({m_last_draw_cover_key,
                                       m_last_draw_signature_key});

        /* A pass identifies the same draw by one signature, because the targets are those of the pass. */
        if (signature_pair.second        == false         &&
            signature_pair.first->second != signature_key)
        {
            m_model.n_key_collisions++;
        }

        /* The calls of a draw begin with its own call, unless the front end says that they begin earlier (the first call is never later than the call of the draw). */
        const bool     has_first_call = (in_draw.first_call_index <= m_call_index);
        const uint64_t first_call     = (has_first_call) ? in_draw.first_call_index
                                                         : m_call_index;

        /* The open pass is not complete yet, so its index is the index that the pass will have in the frame. */
        m_frame.draw_instance_vec.push_back({m_call_index,
                                             m_last_draw_cover_key,
                                             first_call,
                                             static_cast<uint32_t>(m_frame.pass_instance_vec.size() ),
                                             key,
                                             in_draw.view_anchor_call_index});
    }

    open_pass_ptr->has_draws            = true;
    open_pass_ptr->last_draw_call_index = m_call_index;

    open_pass_ptr->n_draws    += 1;
    open_pass_ptr->n_vertices += in_draw.vertex_count;
}

void Analysis::APIDumpAnalysis::ModelBuilder::on_frame_presented()
{
    FrameDetail           frame_detail;
    FrameRecord           frame_record;
    FrameShape            frame_shape;
    std::vector<uint64_t> pass_key_vec;
    uint64_t              shape_key    = 0;

    close_pass();

    for (const auto& instance : m_frame.pass_instance_vec)
    {
        frame_shape.pass_structure_id_vec.push_back(instance.structure_id);
        pass_key_vec.push_back                     (m_model.pass_structure_interner.get_structure(instance.structure_id).key);

        frame_record.n_draws += instance.n_draws;
    }

    std::sort(frame_shape.pass_structure_id_vec.begin(),
              frame_shape.pass_structure_id_vec.end() );

    frame_record.begin_timestamp_qpc = m_frame.begin_timestamp_qpc;
    frame_record.end_timestamp_qpc   = m_timestamp_qpc;
    frame_record.first_call_index    = m_frame.first_call_index;
    frame_record.frame_index         = m_n_frames;
    frame_record.last_call_index     = m_call_index;
    frame_record.n_passes            = static_cast<uint32_t>              (m_frame.pass_instance_vec.size() );
    shape_key                        = MatchKeyBuilder::compute_key_set_key    (pass_key_vec);
    frame_record.shape_id            = m_model.frame_shape_interner.intern(frame_shape,
                                                                           shape_key);

    merge_depth_histograms(frame_record.shape_id,
                           m_frame.depth_histogram_vec,
                           m_frame.pass_instance_vec,
                          &m_model.depth_histogram_map);
    merge_depth_histograms(frame_record.shape_id,
                           m_frame.surface_depth_histogram_vec,
                           m_frame.pass_instance_vec,
                          &m_model.surface_depth_histogram_map);

    for (size_t n_pass = 0;
                n_pass < m_frame.projection_scale_vec.size();
              ++n_pass)
    {
        const auto& scale = m_frame.projection_scale_vec.at(n_pass);

        if (scale.weight != 0)
        {
            auto& total = m_model.projection_scale_map[DepthHistogramKey(frame_record.shape_id,
                                                                         m_frame.pass_instance_vec.at(n_pass).structure_id)];
            const double weight_sum = static_cast<double>(total.weight) + static_cast<double>(scale.weight);

            total.mean   = static_cast<float>( (static_cast<double>(total.mean) * static_cast<double>(total.weight) + static_cast<double>(scale.mean) * static_cast<double>(scale.weight) ) / weight_sum);
            total.weight += scale.weight;
        }
    }

    m_completed_frame.first_call_index  = frame_record.first_call_index;
    m_completed_frame.frame_index       = frame_record.frame_index;
    m_completed_frame.last_call_index   = frame_record.last_call_index;
    m_completed_frame.pass_instance_vec = m_frame.pass_instance_vec;
    m_completed_frame.pass_is_presented_vec = m_frame.pass_is_presented_vec;

    /* The swap hands the old draw instances of the completed frame to m_frame, which is cleared with the rest of the frame at the end. */
    m_completed_frame.draw_instance_vec.swap(m_frame.draw_instance_vec);

    m_completed_frame.pass_key_vec = pass_key_vec;
    m_completed_frame.shape_key    = shape_key;

    if (m_keeps_history)
    {
        m_model.frame_record_vec.push_back(frame_record);
    }

    m_access_tracker.end_frame(frame_record.frame_index,
                              &m_model.resource_access_vec,
                              &m_completed_frame.loop_carried_resource_id_vec);

    /* Details are recorded only for the frames that the store keeps. */
    if (m_keeps_history)
    {
        const bool is_wanted = m_model.representative_store.wants(frame_record.shape_id,
                                                                  frame_record.n_draws);

        if (is_wanted)
        {
            frame_detail.pass_instance_vec   = m_frame.pass_instance_vec;
            frame_detail.transfer_record_vec = m_frame.transfer_record_vec;

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

    m_frame.depth_histogram_vec.clear        ();
    m_frame.draw_instance_vec.clear          ();
    m_frame.pass_instance_vec.clear          ();
    m_frame.pass_is_presented_vec.clear      ();
    m_frame.projection_scale_vec.clear       ();
    m_frame.surface_depth_histogram_vec.clear();
    m_frame.transfer_record_vec.clear        ();
    m_pending_forget_vec.clear       ();

    m_frame.has_calls        = false;
    m_frame.has_seen_3d_draw = false;

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
    OpenPass* open_pass_ptr = m_pass_tracker.get_open_pass_ptr();

    if (open_pass_ptr != nullptr          &&
        open_pass_ptr->has_draws          &&
        (in_viewport == m_viewport) == false)
    {
        close_pass();
    }

    m_viewport = in_viewport;

    /* The pass that was open may be the one that was just closed, so the open pass is looked up again. */
    open_pass_ptr = m_pass_tracker.get_open_pass_ptr();

    if (open_pass_ptr != nullptr)
    {
        open_pass_ptr->viewport = in_viewport;
    }
}

void Analysis::APIDumpAnalysis::ModelBuilder::on_texture_event(const Analysis::APIDumpTextures::TextureEvent& in_event,
                                                               const uint8_t*                                 in_data_ptr,
                                                               const size_t&                                  in_n_data_bytes,
                                                               const uint32_t&                                in_data_chunk_id)
{
    if (m_texture_sink_ptr != nullptr)
    {
        Analysis::APIDumpTextures::TextureEvent event = in_event;

        event.call_index  = m_call_index;
        event.frame_index = m_n_frames;

        m_texture_sink_ptr->on_texture_event(event,
                                             in_data_ptr,
                                             in_n_data_bytes,
                                             in_data_chunk_id);
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

        /* Assigned in place, so the buffers of the previous update are reused. */
        state.block_key = m_key_table.get_unwritten_key(in_update.location.block_id);
        state.update    = in_update;

        m_constant_state_epoch++;
    }
}

void Analysis::APIDumpAnalysis::ModelBuilder::on_upload(const ResourceView&    in_destination,
                                                        const DataReference&   in_data,
                                                        const AccessCoverage&  in_coverage,
                                                        const AccessRectangle& in_rectangle)
{
    Utils::Hashing::StableHasher hasher;
    const SubresourceRange destination_range = get_subresource_range(in_destination);

    m_access_tracker.on_write(in_destination.resource_id);

    emit_access(in_destination.resource_id,
                AccessRole::UPLOAD_DESTINATION,
                AccessMode::WRITE,
                in_coverage, /* UNKNOWN unless the front end says how much of it is given the data */
                destination_range,
                in_rectangle);

    close_pass_if_rendered_to(in_destination);

    /* The contents vary per frame and are not hashed; only the fact that an upload happened is. */
    hasher.add_u32(static_cast<uint32_t>(TransferKind::UPLOAD) );

    m_key_table.record_write(in_destination.resource_id,
                             hasher.get_digest() );

    {
        TransferRecord record;

        record.call_index  = m_call_index;
        record.data        = in_data;
        record.destination = in_destination;
        record.kind        = TransferKind::UPLOAD;

        m_frame.transfer_record_vec.push_back(record);
    }
}

void Analysis::APIDumpAnalysis::ModelBuilder::open_pass_if_needed()
{
    m_pass_tracker.open_if_needed(m_attachments,
                                  m_call_index,
                                  m_timestamp_qpc,
                                  m_viewport);
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

    /* What the sink is told: first the reads, then the writes. A texture is read as a whole (the filter also reaches the levels around the
     * requested one). An attachment is written where the draw reaches, which is not known here.
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

void Analysis::APIDumpAnalysis::ModelBuilder::set_access_sink(IAccessSink* in_sink_ptr)
{
    m_access_sink_ptr = in_sink_ptr;
}

void Analysis::APIDumpAnalysis::ModelBuilder::set_caches_draw_keys(const bool& in_caches_draw_keys)
{
    m_caches_draw_keys = in_caches_draw_keys;
}

void Analysis::APIDumpAnalysis::ModelBuilder::set_texture_sink(Analysis::APIDumpTextures::ITextureSink* in_sink_ptr)
{
    m_texture_sink_ptr = in_sink_ptr;
}

Analysis::APIDumpAnalysis::Model Analysis::APIDumpAnalysis::ModelBuilder::take_model()
{
    return std::move(m_model);
}

bool Analysis::APIDumpAnalysis::ModelBuilder::wants_geometry() const
{
    /* One frame in FRAME_STRIDE gets a geometry sample (with a stride of 1, every frame). A sample is at most a few dozen vertices, and the front end keeps the ones it has taken. */
    constexpr uint64_t FRAME_STRIDE = 1;

    return (m_keeps_history &&
            m_n_frames % FRAME_STRIDE == 0);
}

bool Analysis::APIDumpAnalysis::ModelBuilder::wants_texture_events() const
{
    return (m_texture_sink_ptr != nullptr);
}
