/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_analysis/frontends/fixed_function_state.h"
#include "analysis/api_dump_analysis/hasher.h"

Analysis::APIDumpAnalysis::Frontends::FixedFunctionState::FixedFunctionState(const Rules& in_rules)
    :m_is_state_key_valid(false),
     m_rules             (in_rules),
     m_state_key         (0)
{
    /* Stub */
}

uint64_t Analysis::APIDumpAnalysis::Frontends::FixedFunctionState::compute_state_key() const
{
    Analysis::APIDumpAnalysis::Hasher hasher;

    hasher.add_u64(static_cast<uint64_t>(m_render_state_map.size() ));

    for (const auto& state_pair : m_render_state_map)
    {
        hasher.add_u32(state_pair.first);
        hasher.add_u32((m_rules.is_render_state_value_irrelevant_func_ptr(state_pair.first) ) ? 0
                                                                                              : state_pair.second);
    }

    hasher.add_u64(static_cast<uint64_t>(m_texture_stage_state_map.size() ));

    for (const auto& state_pair : m_texture_stage_state_map)
    {
        hasher.add_u32(state_pair.first.first);
        hasher.add_u32(state_pair.first.second);
        hasher.add_u32((m_rules.is_texture_stage_state_value_irrelevant_func_ptr(state_pair.first.second) ) ? 0
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

Analysis::APIDumpAnalysis::DrawDescription Analysis::APIDumpAnalysis::Frontends::FixedFunctionState::describe_draw(const Analysis::APIDumpAnalysis::PrimitiveTopology& in_topology,
                                                                                                                   const uint32_t&                                     in_vertex_count,
                                                                                                                   const bool&                                         in_is_indexed)
{
    const bool                       is_blending     = (get_render_state_value(m_rules.blend_enable_state,
                                                                               m_rules.blend_enable_default)   != 0);
    const bool                       is_depth_test   = (get_render_state_value(m_rules.depth_enable_state,
                                                                               m_rules.depth_enable_default)   != 0);
    const bool                       is_stencil_test = (get_render_state_value(m_rules.stencil_enable_state,
                                                                               m_rules.stencil_enable_default) != 0);
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

uint32_t Analysis::APIDumpAnalysis::Frontends::FixedFunctionState::get_render_state_value(const uint32_t& in_state,
                                                                                          const uint32_t& in_default) const
{
    const auto state_iterator = m_render_state_map.find(in_state);

    return (state_iterator != m_render_state_map.end() ) ? state_iterator->second
                                                         : in_default;
}

void Analysis::APIDumpAnalysis::Frontends::FixedFunctionState::reset()
{
    *this = FixedFunctionState(m_rules);
}

bool Analysis::APIDumpAnalysis::Frontends::FixedFunctionState::set_light(const uint32_t& in_index,
                                                                         const uint32_t& in_type)
{
    const bool result = (in_index < m_rules.n_max_lights);

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

bool Analysis::APIDumpAnalysis::Frontends::FixedFunctionState::set_light_enable(const uint32_t& in_index,
                                                                                const bool&     in_is_enabled)
{
    const bool result = (in_index < m_rules.n_max_lights);

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

bool Analysis::APIDumpAnalysis::Frontends::FixedFunctionState::set_render_state(const uint32_t& in_state,
                                                                                const uint32_t& in_value)
{
    const bool result = (in_state < m_rules.n_max_render_states);

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

bool Analysis::APIDumpAnalysis::Frontends::FixedFunctionState::set_texture(const uint32_t&                                in_stage,
                                                                           const Analysis::APIDumpAnalysis::ResourceView& in_view)
{
    const bool result = (in_stage < m_rules.n_max_stages);

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

bool Analysis::APIDumpAnalysis::Frontends::FixedFunctionState::set_texture_stage_state(const uint32_t& in_stage,
                                                                                       const uint32_t& in_state,
                                                                                       const uint32_t& in_value)
{
    const bool result = (in_stage < m_rules.n_max_stages      &&
                         in_state < m_rules.n_max_stage_states);

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
