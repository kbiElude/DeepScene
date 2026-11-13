/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_analysis/frontends/OpenGL/gl_pipeline_state.h"
#include "utils/hashing/hasher.h"

uint64_t Analysis::APIDumpAnalysis::Frontends::GLPipelineState::compute_state_key() const
{
    Utils::Hashing::Hasher hasher;

    /* The sets and the maps are in order, so the same state is the same digest whichever way it was reached. */
    for (const auto& capability : m_enabled_capability_set)
    {
        hasher.add_u64(capability);
    }

    for (const auto& value_pair : m_value_map)
    {
        hasher.add_u32(static_cast<uint32_t>(value_pair.first) );
        hasher.add_u32(value_pair.second);
    }

    return hasher.get_digest();
}

Analysis::APIDumpAnalysis::DrawDescription Analysis::APIDumpAnalysis::Frontends::GLPipelineState::describe_draw(const Analysis::APIDumpAnalysis::PrimitiveTopology& in_topology,
                                                                                                                const uint32_t&                                     in_vertex_count) const
{
    constexpr uint32_t BLEND        = 0x0BE2;
    constexpr uint32_t DEPTH_TEST   = 0x0B71;
    constexpr uint32_t STENCIL_TEST = 0x0B90;

    const bool is_blend_enabled   = (m_enabled_capability_set.count(BLEND)        != 0);
    const bool is_depth_enabled   = (m_enabled_capability_set.count(DEPTH_TEST)   != 0);
    const bool is_stencil_enabled = (m_enabled_capability_set.count(STENCIL_TEST) != 0);

    Analysis::APIDumpAnalysis::DrawDescription result;

    result.is_camera_right_handed  = true;
    result.primitive_topology      = in_topology;
    result.reads_color_attachments = is_blend_enabled;
    result.reads_depth_stencil     = (is_depth_enabled ||
                                      is_stencil_enabled);
    result.state_key               = compute_state_key();
    result.vertex_count            = in_vertex_count;

    return result;
}

uint32_t Analysis::APIDumpAnalysis::Frontends::GLPipelineState::get_capability_groups(const uint32_t& in_capability)
{
    constexpr uint32_t COLOR_BUFFER_BIT   = 0x00004000;
    constexpr uint32_t DEPTH_BUFFER_BIT   = 0x00000100;
    constexpr uint32_t ENABLE_BIT         = 0x00002000;
    constexpr uint32_t FOG_BIT            = 0x00000080;
    constexpr uint32_t LIGHTING_BIT       = 0x00000040;
    constexpr uint32_t POLYGON_BIT        = 0x00000008;
    constexpr uint32_t STENCIL_BUFFER_BIT = 0x00000400;
    constexpr uint32_t TEXTURE_BIT        = 0x00040000;
    constexpr uint32_t TRANSFORM_BIT      = 0x00001000;

    /* The capabilities are in the enable group, and each in the group of what it is about (the first of GL_LIGHT0 to GL_LIGHT7 is 0x4000). */
    constexpr uint32_t FIRST_LIGHT = 0x4000;
    constexpr uint32_t N_LIGHTS    = 8;

    uint32_t result = ENABLE_BIT;

    switch (in_capability)
    {
        case 0x0BC0: /* GL_ALPHA_TEST        */
        case 0x0BD0: /* GL_DITHER            */
        case 0x0BE2: /* GL_BLEND             */
        case 0x0BF1: /* GL_INDEX_LOGIC_OP    */
        case 0x0BF2: /* GL_COLOR_LOGIC_OP    */
        {
            result |= COLOR_BUFFER_BIT;

            break;
        }

        case 0x0B71: /* GL_DEPTH_TEST        */
        {
            result |= DEPTH_BUFFER_BIT;

            break;
        }

        case 0x0B90: /* GL_STENCIL_TEST      */
        {
            result |= STENCIL_BUFFER_BIT;

            break;
        }

        case 0x0B41: /* GL_POLYGON_SMOOTH       */
        case 0x0B42: /* GL_POLYGON_STIPPLE      */
        case 0x0B44: /* GL_CULL_FACE            */
        case 0x2A01: /* GL_POLYGON_OFFSET_POINT */
        case 0x2A02: /* GL_POLYGON_OFFSET_LINE  */
        case 0x8037: /* GL_POLYGON_OFFSET_FILL  */
        {
            result |= POLYGON_BIT;

            break;
        }

        case 0x0B50: /* GL_LIGHTING          */
        case 0x0B57: /* GL_COLOR_MATERIAL    */
        {
            result |= LIGHTING_BIT;

            break;
        }

        case 0x0B60: /* GL_FOG               */
        {
            result |= FOG_BIT;

            break;
        }

        case 0x0C60: /* GL_TEXTURE_GEN_S     */
        case 0x0C61: /* GL_TEXTURE_GEN_T     */
        case 0x0C62: /* GL_TEXTURE_GEN_R     */
        case 0x0C63: /* GL_TEXTURE_GEN_Q     */
        case 0x0DE0: /* GL_TEXTURE_1D        */
        case 0x0DE1: /* GL_TEXTURE_2D        */
        case 0x806F: /* GL_TEXTURE_3D        */
        {
            result |= TEXTURE_BIT;

            break;
        }

        case 0x0BA1: /* GL_NORMALIZE         */
        {
            result |= TRANSFORM_BIT;

            break;
        }

        default:
        {
            if (in_capability >= FIRST_LIGHT &&
                in_capability <  FIRST_LIGHT + N_LIGHTS)
            {
                result |= LIGHTING_BIT;
            }

            break;
        }
    }

    return result;
}

uint64_t Analysis::APIDumpAnalysis::Frontends::GLPipelineState::get_unit_key(const uint32_t& in_unit,
                                                                             const uint32_t& in_capability)
{
    /* The unit is in the upper half, from 1 on, so that no key of a unit is a key of a capability that has none. */
    return (static_cast<uint64_t>(in_unit) + 1) << 32 | in_capability;
}

uint32_t Analysis::APIDumpAnalysis::Frontends::GLPipelineState::get_value_groups(const GLStateValue& in_value)
{
    constexpr uint32_t COLOR_BUFFER_BIT = 0x00004000;
    constexpr uint32_t DEPTH_BUFFER_BIT = 0x00000100;
    constexpr uint32_t LIGHTING_BIT     = 0x00000040;
    constexpr uint32_t POLYGON_BIT      = 0x00000008;

    uint32_t result = 0;

    switch (in_value)
    {
        case GLStateValue::ALPHA_FUNC:
        case GLStateValue::BLEND_DESTINATION:
        case GLStateValue::BLEND_SOURCE:
        case GLStateValue::COLOR_MASK:
        {
            result = COLOR_BUFFER_BIT;

            break;
        }

        case GLStateValue::CULL_FACE:
        case GLStateValue::POLYGON_MODE_BACK:
        case GLStateValue::POLYGON_MODE_FRONT:
        {
            result = POLYGON_BIT;

            break;
        }

        case GLStateValue::DEPTH_FUNC:
        case GLStateValue::DEPTH_MASK:
        {
            result = DEPTH_BUFFER_BIT;

            break;
        }

        case GLStateValue::SHADE_MODEL:
        {
            result = LIGHTING_BIT;

            break;
        }

        default:
        {
            break;
        }
    }

    return result;
}

bool Analysis::APIDumpAnalysis::Frontends::GLPipelineState::is_unit_capability(const uint32_t& in_capability)
{
    bool result = false;

    switch (in_capability)
    {
        case 0x0C60: /* GL_TEXTURE_GEN_S    */
        case 0x0C61: /* GL_TEXTURE_GEN_T    */
        case 0x0C62: /* GL_TEXTURE_GEN_R    */
        case 0x0C63: /* GL_TEXTURE_GEN_Q    */
        case 0x0DE0: /* GL_TEXTURE_1D       */
        case 0x0DE1: /* GL_TEXTURE_2D       */
        case 0x806F: /* GL_TEXTURE_3D       */
        case 0x8513: /* GL_TEXTURE_CUBE_MAP */
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

bool Analysis::APIDumpAnalysis::Frontends::GLPipelineState::is_unit_capability_enabled(const uint32_t& in_unit,
                                                                                       const uint32_t& in_capability) const
{
    const uint64_t key = get_unit_key(in_unit,
                                      in_capability);

    return (m_enabled_capability_set.count(key) != 0);
}

void Analysis::APIDumpAnalysis::Frontends::GLPipelineState::restore(const GLPipelineState& in_saved,
                                                                    const uint32_t&        in_mask)
{
    std::set<uint64_t>               capability_set = m_enabled_capability_set;
    std::map<GLStateValue, uint32_t> value_map      = m_value_map;

    /* What is enabled now, and what was when the state was saved: the capability of a group of the mask is as it was. */
    capability_set.insert(in_saved.m_enabled_capability_set.begin(),
                          in_saved.m_enabled_capability_set.end() );

    for (const auto& capability : capability_set)
    {
        const uint32_t groups = get_capability_groups(static_cast<uint32_t>(capability) );

        if ( (groups & in_mask) != 0)
        {
            set_key(capability,
                    (in_saved.m_enabled_capability_set.count(capability) != 0) );
        }
    }

    for (const auto& value_pair : in_saved.m_value_map)
    {
        value_map.insert(value_pair);
    }

    for (const auto& value_pair : value_map)
    {
        if ( (get_value_groups(value_pair.first) & in_mask) != 0)
        {
            const auto saved_iterator = in_saved.m_value_map.find(value_pair.first);
            const auto saved_end      = in_saved.m_value_map.end();

            if (saved_iterator != saved_end)
            {
                m_value_map[value_pair.first] = saved_iterator->second;
            }
            else
            {
                m_value_map.erase(value_pair.first);
            }
        }
    }
}

void Analysis::APIDumpAnalysis::Frontends::GLPipelineState::set_capability(const uint32_t& in_capability,
                                                                           const bool&     in_is_enabled)
{
    set_key(in_capability,
            in_is_enabled);
}

void Analysis::APIDumpAnalysis::Frontends::GLPipelineState::set_key(const uint64_t& in_key,
                                                                    const bool&     in_is_enabled)
{
    if (in_is_enabled)
    {
        m_enabled_capability_set.insert(in_key);
    }
    else
    {
        m_enabled_capability_set.erase(in_key);
    }
}

void Analysis::APIDumpAnalysis::Frontends::GLPipelineState::set_unit_capability(const uint32_t& in_unit,
                                                                                const uint32_t& in_capability,
                                                                                const bool&     in_is_enabled)
{
    const uint64_t key = get_unit_key(in_unit,
                                      in_capability);

    set_key(key,
            in_is_enabled);
}

void Analysis::APIDumpAnalysis::Frontends::GLPipelineState::set_value(const GLStateValue& in_value,
                                                                      const uint32_t&     in_setting)
{
    m_value_map[in_value] = in_setting;
}
