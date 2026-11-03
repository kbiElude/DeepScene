/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "Analysis/APIDumpAnalysis/frontends/d3d7_pipeline_state.h"
#include "Analysis/APIDumpAnalysis/hasher.h"
#include "Interception/Core/D3D7Utils/constants.h"

/* What a device can have: an index beyond these is not one that D3D7 has. */
static constexpr uint32_t N_MAX_LIGHTS        = 256;
static constexpr uint32_t N_MAX_RENDER_STATES = 256;
static constexpr uint32_t N_MAX_STAGE_STATES  = 32;
static constexpr uint32_t N_MAX_STAGES        = 8;

Analysis::APIDumpAnalysis::Frontends::D3D7PipelineState::D3D7PipelineState()
    :m_fixed_function_state(get_rules() )
{
    /* Stub */
}

Analysis::APIDumpAnalysis::DrawDescription Analysis::APIDumpAnalysis::Frontends::D3D7PipelineState::describe_draw(const Analysis::APIDumpAnalysis::PrimitiveTopology& in_topology,
                                                                                                                  const uint32_t&                                     in_vertex_count,
                                                                                                                  const bool&                                         in_is_indexed,
                                                                                                                  const uint32_t&                                     in_fvf,
                                                                                                                  const Analysis::APIDumpAnalysis::ResourceId&        in_vertex_buffer_id)
{
    Analysis::APIDumpAnalysis::Hasher          layout_hasher;
    Analysis::APIDumpAnalysis::DrawDescription result = m_fixed_function_state.describe_draw(in_topology,
                                                                                             in_vertex_count,
                                                                                             in_is_indexed);

    /* The format of the vertices is the format code. Where they come from is told by the vertex buffer, if there is one. */
    layout_hasher.add_u32(in_fvf);

    if (in_vertex_buffer_id != Analysis::APIDumpAnalysis::ResourceId::NONE)
    {
        result.vertex_buffer_id_vec.push_back(in_vertex_buffer_id);
    }

    result.vertex_layout_key = layout_hasher.get_digest();

    return result;
}

Analysis::APIDumpAnalysis::Frontends::FixedFunctionState::Rules Analysis::APIDumpAnalysis::Frontends::D3D7PipelineState::get_rules()
{
    constexpr uint32_t BLEND_DEFAULT   = 0; /* off */
    constexpr uint32_t DEPTH_DEFAULT   = 1; /* on, with a depth buffer */
    constexpr uint32_t STENCIL_DEFAULT = 0; /* off */

    FixedFunctionState::Rules result;

    result.blend_enable_default                             = BLEND_DEFAULT;
    result.blend_enable_state                               = Interception::Core::D3D7Utils::D3DRENDERSTATE_ALPHABLENDENABLE;
    result.depth_enable_default                             = DEPTH_DEFAULT;
    result.depth_enable_state                               = Interception::Core::D3D7Utils::D3DRENDERSTATE_ZENABLE;
    result.is_render_state_value_irrelevant_func_ptr        = &is_value_irrelevant_render_state;
    result.is_texture_stage_state_value_irrelevant_func_ptr = &is_value_irrelevant_texture_stage_state;
    result.n_max_lights                                     = N_MAX_LIGHTS;
    result.n_max_render_states                              = N_MAX_RENDER_STATES;
    result.n_max_stage_states                               = N_MAX_STAGE_STATES;
    result.n_max_stages                                     = N_MAX_STAGES;
    result.stencil_enable_default                           = STENCIL_DEFAULT;
    result.stencil_enable_state                             = Interception::Core::D3D7Utils::D3DRENDERSTATE_STENCILENABLE;

    return result;
}

bool Analysis::APIDumpAnalysis::Frontends::D3D7PipelineState::is_value_irrelevant_render_state(const uint32_t& in_state)
{
    bool result = false;

    switch (in_state)
    {
        case Interception::Core::D3D7Utils::D3DRENDERSTATE_ALPHAREF:
        case Interception::Core::D3D7Utils::D3DRENDERSTATE_AMBIENT:
        case Interception::Core::D3D7Utils::D3DRENDERSTATE_ANISOTROPY:
        case Interception::Core::D3D7Utils::D3DRENDERSTATE_BORDERCOLOR:
        case Interception::Core::D3D7Utils::D3DRENDERSTATE_FOGCOLOR:
        case Interception::Core::D3D7Utils::D3DRENDERSTATE_FOGDENSITY:
        case Interception::Core::D3D7Utils::D3DRENDERSTATE_FOGEND:
        case Interception::Core::D3D7Utils::D3DRENDERSTATE_FOGSTART:
        case Interception::Core::D3D7Utils::D3DRENDERSTATE_LINEPATTERN:
        case Interception::Core::D3D7Utils::D3DRENDERSTATE_MIPMAPLODBIAS:
        case Interception::Core::D3D7Utils::D3DRENDERSTATE_STENCILMASK:
        case Interception::Core::D3D7Utils::D3DRENDERSTATE_STENCILREF:
        case Interception::Core::D3D7Utils::D3DRENDERSTATE_STENCILWRITEMASK:
        case Interception::Core::D3D7Utils::D3DRENDERSTATE_TEXTUREFACTOR:
        case Interception::Core::D3D7Utils::D3DRENDERSTATE_ZBIAS:
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

bool Analysis::APIDumpAnalysis::Frontends::D3D7PipelineState::is_value_irrelevant_texture_stage_state(const uint32_t& in_state)
{
    bool result = false;

    switch (in_state)
    {
        case Interception::Core::D3D7Utils::D3DTSS_BORDERCOLOR:
        case Interception::Core::D3D7Utils::D3DTSS_BUMPENVLOFFSET:
        case Interception::Core::D3D7Utils::D3DTSS_BUMPENVLSCALE:
        case Interception::Core::D3D7Utils::D3DTSS_BUMPENVMAT00:
        case Interception::Core::D3D7Utils::D3DTSS_BUMPENVMAT01:
        case Interception::Core::D3D7Utils::D3DTSS_BUMPENVMAT10:
        case Interception::Core::D3D7Utils::D3DTSS_BUMPENVMAT11:
        case Interception::Core::D3D7Utils::D3DTSS_MAXANISOTROPY:
        case Interception::Core::D3D7Utils::D3DTSS_MAXMIPLEVEL:
        case Interception::Core::D3D7Utils::D3DTSS_MIPMAPLODBIAS:
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

void Analysis::APIDumpAnalysis::Frontends::D3D7PipelineState::reset()
{
    *this = D3D7PipelineState();
}

bool Analysis::APIDumpAnalysis::Frontends::D3D7PipelineState::set_light(const uint32_t& in_index,
                                                                        const uint32_t& in_type)
{
    return m_fixed_function_state.set_light(in_index,
                                            in_type);
}

bool Analysis::APIDumpAnalysis::Frontends::D3D7PipelineState::set_light_enable(const uint32_t& in_index,
                                                                               const bool&     in_is_enabled)
{
    return m_fixed_function_state.set_light_enable(in_index,
                                                   in_is_enabled);
}

bool Analysis::APIDumpAnalysis::Frontends::D3D7PipelineState::set_render_state(const uint32_t& in_state,
                                                                               const uint32_t& in_value)
{
    return m_fixed_function_state.set_render_state(in_state,
                                                   in_value);
}

bool Analysis::APIDumpAnalysis::Frontends::D3D7PipelineState::set_texture(const uint32_t&                                in_stage,
                                                                          const Analysis::APIDumpAnalysis::ResourceView& in_view)
{
    return m_fixed_function_state.set_texture(in_stage,
                                              in_view);
}

bool Analysis::APIDumpAnalysis::Frontends::D3D7PipelineState::set_texture_stage_state(const uint32_t& in_stage,
                                                                                      const uint32_t& in_state,
                                                                                      const uint32_t& in_value)
{
    return m_fixed_function_state.set_texture_stage_state(in_stage,
                                                          in_state,
                                                          in_value);
}
