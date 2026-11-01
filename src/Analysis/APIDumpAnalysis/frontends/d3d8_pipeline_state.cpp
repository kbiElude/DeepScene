/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "Analysis/APIDumpAnalysis/frontends/d3d8_pipeline_state.h"
#include "Analysis/APIDumpAnalysis/hasher.h"
#include "Interception/Hooks/D3D8/types.h"

/* What a device can have: an index beyond these is not one that D3D8 has. */
static constexpr uint32_t N_MAX_LIGHTS        = 256;
static constexpr uint32_t N_MAX_RENDER_STATES = 256;
static constexpr uint32_t N_MAX_STAGE_STATES  = 32;
static constexpr uint32_t N_MAX_STAGES        = 8;
static constexpr uint32_t N_MAX_STREAMS       = 16;

Analysis::APIDumpAnalysis::Frontends::D3D8PipelineState::D3D8PipelineState()
    :m_index_buffer_id    (Analysis::APIDumpAnalysis::ResourceId::NONE),
     m_is_state_key_valid (false),
     m_pixel_shader_id    (Analysis::APIDumpAnalysis::ResourceId::NONE),
     m_state_key          (0),
     m_vertex_layout_value(0),
     m_vertex_shader_id   (Analysis::APIDumpAnalysis::ResourceId::NONE)
{
    /* Stub */
}

uint64_t Analysis::APIDumpAnalysis::Frontends::D3D8PipelineState::compute_state_key() const
{
    Analysis::APIDumpAnalysis::Hasher hasher;

    hasher.add_u64(static_cast<uint64_t>(m_render_state_map.size() ));

    for (const auto& state_pair : m_render_state_map)
    {
        hasher.add_u32(state_pair.first);
        hasher.add_u32((is_value_irrelevant_render_state(state_pair.first) ) ? 0
                                                                             : state_pair.second);
    }

    hasher.add_u64(static_cast<uint64_t>(m_texture_stage_state_map.size() ));

    for (const auto& state_pair : m_texture_stage_state_map)
    {
        hasher.add_u32(state_pair.first.first);
        hasher.add_u32(state_pair.first.second);
        hasher.add_u32((is_value_irrelevant_texture_stage_state(state_pair.first.second) ) ? 0
                                                                                           : state_pair.second);
    }

    /* Which lights are on, and what kind they are. */
    hasher.add_u64(static_cast<uint64_t>(m_enabled_light_set.size() ));

    for (const auto& light_index : m_enabled_light_set)
    {
        const auto type_iterator = m_light_type_map.find(light_index);

        hasher.add_u32(light_index);
        hasher.add_u32((type_iterator != m_light_type_map.end() ) ? type_iterator->second
                                                                  : 0);
    }

    return hasher.get_digest();
}

Analysis::APIDumpAnalysis::DrawDescription Analysis::APIDumpAnalysis::Frontends::D3D8PipelineState::describe_common(const Analysis::APIDumpAnalysis::PrimitiveTopology& in_topology,
                                                                                                                    const uint32_t&                                     in_vertex_count,
                                                                                                                    const bool&                                         in_is_indexed)
{
    constexpr uint32_t BLEND_DEFAULT   = 0; /* off */
    constexpr uint32_t DEPTH_DEFAULT   = 1; /* on  */
    constexpr uint32_t STENCIL_DEFAULT = 0; /* off */

    const bool                       is_blending     = (get_render_state_value(Interception::Hooks::D3D8::D3DRS_ALPHABLENDENABLE,
                                                                               BLEND_DEFAULT)                                     != 0);
    const bool                       is_depth_test   = (get_render_state_value(Interception::Hooks::D3D8::D3DRS_ZENABLE,
                                                                               DEPTH_DEFAULT)                                     != 0);
    const bool                       is_stencil_test = (get_render_state_value(Interception::Hooks::D3D8::D3DRS_STENCILENABLE,
                                                                               STENCIL_DEFAULT)                                   != 0);
    Analysis::APIDumpAnalysis::DrawDescription result;

    if (m_is_state_key_valid == false)
    {
        m_state_key          = compute_state_key();
        m_is_state_key_valid = true;
    }

    result.texture_binding_vec.reserve(m_texture_view_map.size() );

    result.is_indexed              = in_is_indexed;
    result.primitive_topology      = in_topology;
    result.reads_color_attachments = is_blending;
    result.reads_depth_stencil     = (is_depth_test || is_stencil_test);
    result.shader_id_vec           = get_shader_id_vec();
    result.state_key               = m_state_key;
    result.vertex_count            = in_vertex_count;

    for (const auto& texture_pair : m_texture_view_map)
    {
        Analysis::APIDumpAnalysis::TextureBinding binding;

        binding.binding_point.index = texture_pair.first;
        binding.view                = texture_pair.second;

        result.texture_binding_vec.push_back(binding);
    }

    return result;
}

Analysis::APIDumpAnalysis::DrawDescription Analysis::APIDumpAnalysis::Frontends::D3D8PipelineState::describe_draw(const Analysis::APIDumpAnalysis::PrimitiveTopology& in_topology,
                                                                                                                  const uint32_t&                                     in_vertex_count,
                                                                                                                  const bool&                                         in_is_indexed)
{
    Analysis::APIDumpAnalysis::Hasher          layout_hasher;
    Analysis::APIDumpAnalysis::DrawDescription result = describe_common(in_topology,
                                                                        in_vertex_count,
                                                                        in_is_indexed);

    result.index_buffer_id = (in_is_indexed) ? m_index_buffer_id
                                             : Analysis::APIDumpAnalysis::ResourceId::NONE;

    /* The format of the vertices: what the format code or the declaration says, and how far apart the vertices are. */
    layout_hasher.add_u64(m_vertex_layout_value);
    layout_hasher.add_u64(static_cast<uint64_t>(m_stream_binding_map.size() ));

    result.vertex_buffer_id_vec.reserve(m_stream_binding_map.size() );

    for (const auto& stream_pair : m_stream_binding_map)
    {
        layout_hasher.add_u32(stream_pair.first);
        layout_hasher.add_u32(stream_pair.second.stride);

        result.vertex_buffer_id_vec.push_back(stream_pair.second.id);
    }

    result.vertex_layout_key = layout_hasher.get_digest();

    return result;
}

Analysis::APIDumpAnalysis::DrawDescription Analysis::APIDumpAnalysis::Frontends::D3D8PipelineState::describe_user_pointer_draw(const Analysis::APIDumpAnalysis::PrimitiveTopology& in_topology,
                                                                                                                               const uint32_t&                                     in_vertex_count,
                                                                                                                               const bool&                                         in_is_indexed,
                                                                                                                               const uint32_t&                                     in_stride)
{
    Analysis::APIDumpAnalysis::Hasher          layout_hasher;
    Analysis::APIDumpAnalysis::DrawDescription result = describe_common(in_topology,
                                                                        in_vertex_count,
                                                                        in_is_indexed);

    /* The only stream is the application's memory. */
    layout_hasher.add_u64(m_vertex_layout_value);
    layout_hasher.add_u32(in_stride);

    result.vertex_layout_key = layout_hasher.get_digest();

    return result;
}

uint32_t Analysis::APIDumpAnalysis::Frontends::D3D8PipelineState::get_render_state_value(const uint32_t& in_state,
                                                                                         const uint32_t& in_default) const
{
    const auto state_iterator = m_render_state_map.find(in_state);

    return (state_iterator != m_render_state_map.end() ) ? state_iterator->second
                                                         : in_default;
}

std::vector<Analysis::APIDumpAnalysis::ResourceId> Analysis::APIDumpAnalysis::Frontends::D3D8PipelineState::get_shader_id_vec() const
{
    constexpr size_t N_STAGES_WITH_SHADERS = 2; /* vertex, pixel */

    std::vector<Analysis::APIDumpAnalysis::ResourceId> result_vec;

    result_vec.reserve(N_STAGES_WITH_SHADERS);

    if (m_vertex_shader_id != Analysis::APIDumpAnalysis::ResourceId::NONE)
    {
        result_vec.push_back(m_vertex_shader_id);
    }

    if (m_pixel_shader_id != Analysis::APIDumpAnalysis::ResourceId::NONE)
    {
        result_vec.push_back(m_pixel_shader_id);
    }

    return result_vec;
}

bool Analysis::APIDumpAnalysis::Frontends::D3D8PipelineState::is_value_irrelevant_render_state(const uint32_t& in_state)
{
    bool result = false;

    switch (in_state)
    {
        case Interception::Hooks::D3D8::D3DRS_ALPHAREF:
        case Interception::Hooks::D3D8::D3DRS_AMBIENT:
        case Interception::Hooks::D3D8::D3DRS_FOGCOLOR:
        case Interception::Hooks::D3D8::D3DRS_FOGDENSITY:
        case Interception::Hooks::D3D8::D3DRS_FOGEND:
        case Interception::Hooks::D3D8::D3DRS_FOGSTART:
        case Interception::Hooks::D3D8::D3DRS_LINEPATTERN:
        case Interception::Hooks::D3D8::D3DRS_POINTSCALE_A:
        case Interception::Hooks::D3D8::D3DRS_POINTSCALE_B:
        case Interception::Hooks::D3D8::D3DRS_POINTSCALE_C:
        case Interception::Hooks::D3D8::D3DRS_POINTSIZE:
        case Interception::Hooks::D3D8::D3DRS_POINTSIZE_MAX:
        case Interception::Hooks::D3D8::D3DRS_POINTSIZE_MIN:
        case Interception::Hooks::D3D8::D3DRS_STENCILMASK:
        case Interception::Hooks::D3D8::D3DRS_STENCILREF:
        case Interception::Hooks::D3D8::D3DRS_STENCILWRITEMASK:
        case Interception::Hooks::D3D8::D3DRS_TEXTUREFACTOR:
        case Interception::Hooks::D3D8::D3DRS_TWEENFACTOR:
        case Interception::Hooks::D3D8::D3DRS_ZBIAS:
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

bool Analysis::APIDumpAnalysis::Frontends::D3D8PipelineState::is_value_irrelevant_texture_stage_state(const uint32_t& in_state)
{
    bool result = false;

    switch (in_state)
    {
        case Interception::Hooks::D3D8::D3DTSS_BORDERCOLOR:
        case Interception::Hooks::D3D8::D3DTSS_BUMPENVLOFFSET:
        case Interception::Hooks::D3D8::D3DTSS_BUMPENVLSCALE:
        case Interception::Hooks::D3D8::D3DTSS_BUMPENVMAT00:
        case Interception::Hooks::D3D8::D3DTSS_BUMPENVMAT01:
        case Interception::Hooks::D3D8::D3DTSS_BUMPENVMAT10:
        case Interception::Hooks::D3D8::D3DTSS_BUMPENVMAT11:
        case Interception::Hooks::D3D8::D3DTSS_MIPMAPLODBIAS:
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

uint32_t Analysis::APIDumpAnalysis::Frontends::D3D8PipelineState::get_fvf() const
{
    return (m_vertex_shader_id == Analysis::APIDumpAnalysis::ResourceId::NONE) ? static_cast<uint32_t>(m_vertex_layout_value)
                                                                               : 0;
}

Analysis::APIDumpAnalysis::ResourceId Analysis::APIDumpAnalysis::Frontends::D3D8PipelineState::get_index_buffer_id() const
{
    return m_index_buffer_id;
}

bool Analysis::APIDumpAnalysis::Frontends::D3D8PipelineState::get_stream_source(const uint32_t&                        in_stream,
                                                                                Analysis::APIDumpAnalysis::ResourceId* out_id_ptr,
                                                                                uint32_t*                              out_stride_ptr) const
{
    const auto binding_iterator = m_stream_binding_map.find(in_stream);
    bool       result           = false;

    if (binding_iterator != m_stream_binding_map.end() )
    {
        *out_id_ptr     = binding_iterator->second.id;
        *out_stride_ptr = binding_iterator->second.stride;

        result = (binding_iterator->second.id != Analysis::APIDumpAnalysis::ResourceId::NONE);
    }

    return result;
}

void Analysis::APIDumpAnalysis::Frontends::D3D8PipelineState::reset()
{
    *this = D3D8PipelineState();
}

void Analysis::APIDumpAnalysis::Frontends::D3D8PipelineState::set_index_buffer(const Analysis::APIDumpAnalysis::ResourceId& in_id)
{
    m_index_buffer_id = in_id;
}

bool Analysis::APIDumpAnalysis::Frontends::D3D8PipelineState::set_light(const uint32_t& in_index,
                                                                        const uint32_t& in_type)
{
    const bool result = (in_index < N_MAX_LIGHTS);

    if (result)
    {
        const auto type_iterator = m_light_type_map.find(in_index);

        if (type_iterator         == m_light_type_map.end() ||
            type_iterator->second != in_type)
        {
            m_light_type_map[in_index] = in_type;
            m_is_state_key_valid       = false;
        }
    }

    return result;
}

bool Analysis::APIDumpAnalysis::Frontends::D3D8PipelineState::set_light_enable(const uint32_t& in_index,
                                                                               const bool&     in_is_enabled)
{
    const bool result = (in_index < N_MAX_LIGHTS);

    if (result)
    {
        const bool was_enabled = (m_enabled_light_set.find(in_index) != m_enabled_light_set.end() );

        if (in_is_enabled != was_enabled)
        {
            if (in_is_enabled)
            {
                m_enabled_light_set.insert(in_index);
            }
            else
            {
                m_enabled_light_set.erase(in_index);
            }

            m_is_state_key_valid = false;
        }
    }

    return result;
}

void Analysis::APIDumpAnalysis::Frontends::D3D8PipelineState::set_pixel_shader(const Analysis::APIDumpAnalysis::ResourceId& in_id)
{
    m_pixel_shader_id = in_id;
}

bool Analysis::APIDumpAnalysis::Frontends::D3D8PipelineState::set_render_state(const uint32_t& in_state,
                                                                               const uint32_t& in_value)
{
    const bool result = (in_state < N_MAX_RENDER_STATES);

    if (result)
    {
        /* Setting a state to what it is (which applications do all the time) leaves the key alone. */
        const auto state_iterator = m_render_state_map.lower_bound(in_state);

        if (state_iterator        == m_render_state_map.end() ||
            state_iterator->first != in_state)
        {
            m_render_state_map.emplace_hint(state_iterator,
                                            in_state,
                                            in_value);

            m_is_state_key_valid = false;
        }
        else
        if (state_iterator->second != in_value)
        {
            state_iterator->second = in_value;
            m_is_state_key_valid   = false;
        }
    }

    return result;
}

bool Analysis::APIDumpAnalysis::Frontends::D3D8PipelineState::set_stream_source(const uint32_t&                              in_stream,
                                                                                const Analysis::APIDumpAnalysis::ResourceId& in_id,
                                                                                const uint32_t&                              in_stride)
{
    const bool result = (in_stream < N_MAX_STREAMS);

    if (result == false)
    {
        /* Nothing to do. */
    }
    else
    if (in_id == Analysis::APIDumpAnalysis::ResourceId::NONE)
    {
        m_stream_binding_map.erase(in_stream);
    }
    else
    {
        StreamBinding binding;

        binding.id     = in_id;
        binding.stride = in_stride;

        m_stream_binding_map[in_stream] = binding;
    }

    return result;
}

bool Analysis::APIDumpAnalysis::Frontends::D3D8PipelineState::set_texture(const uint32_t&                                in_stage,
                                                                          const Analysis::APIDumpAnalysis::ResourceView& in_view)
{
    const bool result = (in_stage < N_MAX_STAGES);

    if (result == false)
    {
        /* Nothing to do. */
    }
    else
    if (in_view.resource_id == Analysis::APIDumpAnalysis::ResourceId::NONE)
    {
        m_texture_view_map.erase(in_stage);
    }
    else
    {
        m_texture_view_map[in_stage] = in_view;
    }

    return result;
}

bool Analysis::APIDumpAnalysis::Frontends::D3D8PipelineState::set_texture_stage_state(const uint32_t& in_stage,
                                                                                      const uint32_t& in_state,
                                                                                      const uint32_t& in_value)
{
    const bool result = (in_stage < N_MAX_STAGES      &&
                         in_state < N_MAX_STAGE_STATES);

    if (result)
    {
        const StageStateKey key(in_stage,
                                in_state);
        const auto          state_iterator = m_texture_stage_state_map.lower_bound(key);

        if (state_iterator        == m_texture_stage_state_map.end() ||
            state_iterator->first != key)
        {
            m_texture_stage_state_map.emplace_hint(state_iterator,
                                                   key,
                                                   in_value);

            m_is_state_key_valid = false;
        }
        else
        if (state_iterator->second != in_value)
        {
            state_iterator->second = in_value;
            m_is_state_key_valid   = false;
        }
    }

    return result;
}

void Analysis::APIDumpAnalysis::Frontends::D3D8PipelineState::set_vertex_shader(const uint32_t& in_fvf)
{
    m_vertex_layout_value = in_fvf;
    m_vertex_shader_id    = Analysis::APIDumpAnalysis::ResourceId::NONE;
}

void Analysis::APIDumpAnalysis::Frontends::D3D8PipelineState::set_vertex_shader(const Analysis::APIDumpAnalysis::ResourceId& in_id,
                                                                                const uint64_t&                              in_declaration_hash)
{
    m_vertex_layout_value = in_declaration_hash;
    m_vertex_shader_id    = in_id;
}
