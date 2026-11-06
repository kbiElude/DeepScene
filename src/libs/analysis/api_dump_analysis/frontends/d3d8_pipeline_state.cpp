/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_analysis/frontends/d3d8_pipeline_state.h"
#include "utils/hashing/hasher.h"
#include "Interception/Hooks/D3D8/types.h"

/* What a device can have: an index beyond these is not one that D3D8 has. */
static constexpr uint32_t N_MAX_LIGHTS        = 256;
static constexpr uint32_t N_MAX_RENDER_STATES = 256;
static constexpr uint32_t N_MAX_STAGE_STATES  = 32;
static constexpr uint32_t N_MAX_STAGES        = 8;
static constexpr uint32_t N_MAX_STREAMS       = 16;

Analysis::APIDumpAnalysis::Frontends::D3D8PipelineState::D3D8PipelineState()
    :m_fixed_function_state(get_rules() ),
     m_index_buffer_id     (Analysis::APIDumpAnalysis::ResourceId::NONE),
     m_pixel_shader_id     (Analysis::APIDumpAnalysis::ResourceId::NONE),
     m_vertex_layout_value (0),
     m_vertex_shader_id    (Analysis::APIDumpAnalysis::ResourceId::NONE)
{
    /* Stub */
}

Analysis::APIDumpAnalysis::DrawDescription Analysis::APIDumpAnalysis::Frontends::D3D8PipelineState::describe_draw(const Analysis::APIDumpAnalysis::PrimitiveTopology& in_topology,
                                                                                                                  const uint32_t&                                     in_vertex_count,
                                                                                                                  const bool&                                         in_is_indexed)
{
    Utils::Hashing::Hasher                     layout_hasher;
    Analysis::APIDumpAnalysis::DrawDescription result        = m_fixed_function_state.describe_draw(in_topology,
                                                                                                    in_vertex_count,
                                                                                                    in_is_indexed);

    result.index_buffer_id                                 = (in_is_indexed) ? m_index_buffer_id
                                             : Analysis::APIDumpAnalysis::ResourceId::NONE;
    result.shader_id_vec                                   = get_shader_id_vec();

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

    result.vertex_layout_key                               = layout_hasher.get_digest();

    return result;
}

Analysis::APIDumpAnalysis::DrawDescription Analysis::APIDumpAnalysis::Frontends::D3D8PipelineState::describe_user_pointer_draw(const Analysis::APIDumpAnalysis::PrimitiveTopology& in_topology,
                                                                                                                               const uint32_t&                                     in_vertex_count,
                                                                                                                               const bool&                                         in_is_indexed,
                                                                                                                               const uint32_t&                                     in_stride)
{
    Utils::Hashing::Hasher                     layout_hasher;
    Analysis::APIDumpAnalysis::DrawDescription result        = m_fixed_function_state.describe_draw(in_topology,
                                                                                                    in_vertex_count,
                                                                                                    in_is_indexed);

    result.shader_id_vec                                   = get_shader_id_vec();

    /* The only stream is the application's memory. */
    layout_hasher.add_u64(m_vertex_layout_value);
    layout_hasher.add_u32(in_stride);

    result.vertex_layout_key                               = layout_hasher.get_digest();

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

Analysis::APIDumpAnalysis::Frontends::FixedFunctionState::Rules Analysis::APIDumpAnalysis::Frontends::D3D8PipelineState::get_rules()
{
    constexpr uint32_t BLEND_DEFAULT   = 0; /* off */
    constexpr uint32_t DEPTH_DEFAULT   = 1; /* on  */
    constexpr uint32_t STENCIL_DEFAULT = 0; /* off */

    FixedFunctionState::Rules result;

    result.blend_enable_default                            = BLEND_DEFAULT;
    result.blend_enable_state                              = Interception::Hooks::D3D8::D3DRS_ALPHABLENDENABLE;
    result.depth_enable_default                            = DEPTH_DEFAULT;
    result.depth_enable_state                              = Interception::Hooks::D3D8::D3DRS_ZENABLE;
    result.is_render_state_value_irrelevant_func_ptr       = &is_value_irrelevant_render_state;
    result.is_texture_stage_state_value_irrelevant_func_ptr = &is_value_irrelevant_texture_stage_state;
    result.n_max_lights                                    = N_MAX_LIGHTS;
    result.n_max_render_states                             = N_MAX_RENDER_STATES;
    result.n_max_stage_states                              = N_MAX_STAGE_STATES;
    result.n_max_stages                                    = N_MAX_STAGES;
    result.stencil_enable_default                          = STENCIL_DEFAULT;
    result.stencil_enable_state                            = Interception::Hooks::D3D8::D3DRS_STENCILENABLE;

    return result;
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

Analysis::APIDumpAnalysis::Frontends::FixedFunctionState& Analysis::APIDumpAnalysis::Frontends::D3D8PipelineState::get_fixed_function_state()
{
    return m_fixed_function_state;
}

void Analysis::APIDumpAnalysis::Frontends::D3D8PipelineState::reset()
{
    *this = D3D8PipelineState();
}

void Analysis::APIDumpAnalysis::Frontends::D3D8PipelineState::set_index_buffer(const Analysis::APIDumpAnalysis::ResourceId& in_id)
{
    m_index_buffer_id = in_id;
}

void Analysis::APIDumpAnalysis::Frontends::D3D8PipelineState::set_pixel_shader(const Analysis::APIDumpAnalysis::ResourceId& in_id)
{
    m_pixel_shader_id = in_id;
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
