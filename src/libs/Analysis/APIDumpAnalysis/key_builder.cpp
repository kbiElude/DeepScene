/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "Analysis/APIDumpAnalysis/key_builder.h"
#include "Analysis/APIDumpAnalysis/hasher.h"
#include <algorithm>

uint64_t Analysis::APIDumpAnalysis::KeyBuilder::compute_constant_signature_key(const ConstantSignature& in_signature)
{
    Hasher hasher;

    hasher.add_u64(in_signature.block_key);
    hasher.add_u32(static_cast<uint32_t>(in_signature.block_kind) );
    hasher.add_u64(in_signature.byte_offset);
    hasher.add_u64(in_signature.byte_size);
    hasher.add_u32(static_cast<uint32_t>(in_signature.provenance) );
    hasher.add_u32(static_cast<uint32_t>(in_signature.semantic) );

    return hasher.get_digest();
}

uint64_t Analysis::APIDumpAnalysis::KeyBuilder::compute_description_key(const ResourceDescription& in_description)
{
    Hasher hasher;

    hasher.add_u64 (in_description.api_flags);
    hasher.add_u64 (in_description.content_hash);
    hasher.add_u32 (in_description.depth_or_layer_count);
    hasher.add_u32 (static_cast<uint32_t>(in_description.format.format_class) );
    hasher.add_u32 (in_description.format.raw_value);
    hasher.add_bool(in_description.has_content_hash);
    hasher.add_u32 (in_description.height);
    hasher.add_bool(in_description.is_depth_stencil);
    hasher.add_bool(in_description.is_dynamic);
    hasher.add_bool(in_description.is_render_target);
    hasher.add_u32 (static_cast<uint32_t>(in_description.kind) );
    hasher.add_u32 (in_description.mip_level_count);
    hasher.add_u32 (in_description.sample_count);
    hasher.add_u32 (static_cast<uint32_t>(in_description.shader_stage) );
    hasher.add_u64 (in_description.size_in_bytes);
    hasher.add_u32 (in_description.width);

    return hasher.get_digest();
}

uint64_t Analysis::APIDumpAnalysis::KeyBuilder::compute_draw_cover_key(const uint64_t&          in_signature_key,
                                                                       const uint32_t&          in_vertex_count,
                                                                       const PrimitiveTopology& in_topology)
{
    Hasher hasher;

    hasher.add_u64(in_signature_key);
    hasher.add_u32(in_vertex_count);
    hasher.add_u32(static_cast<uint32_t>(in_topology) );

    return hasher.get_digest();
}

uint64_t Analysis::APIDumpAnalysis::KeyBuilder::compute_draw_signature_key(const uint64_t&              in_unwritten_structure_key,
                                                                           const std::vector<uint64_t>& in_unwritten_color_view_key_vec,
                                                                           const uint64_t&              in_unwritten_depth_stencil_view_key)
{
    Hasher hasher;

    hasher.add_u64(static_cast<uint64_t>(in_unwritten_color_view_key_vec.size() ));

    for (const auto& color_view_key : in_unwritten_color_view_key_vec)
    {
        hasher.add_u64(color_view_key);
    }

    hasher.add_u64(in_unwritten_depth_stencil_view_key);
    hasher.add_u64(in_unwritten_structure_key);

    return hasher.get_digest();
}

uint64_t Analysis::APIDumpAnalysis::KeyBuilder::compute_draw_structure_key(const DrawStructure& in_structure)
{
    Hasher hasher;

    hasher.add_u64 (in_structure.index_buffer_key);
    hasher.add_bool(in_structure.is_indexed);
    hasher.add_bool(in_structure.is_instanced);
    hasher.add_u32 (static_cast<uint32_t>(in_structure.primitive_topology) );
    hasher.add_u64 (static_cast<uint64_t>(in_structure.shader_key_vec.size() ));

    for (const auto& shader_key : in_structure.shader_key_vec)
    {
        hasher.add_u64(shader_key);
    }

    hasher.add_u64(in_structure.state_key);
    hasher.add_u64(static_cast<uint64_t>(in_structure.texture_binding_key_vec.size() ));

    for (const auto& binding : in_structure.texture_binding_key_vec)
    {
        hasher.add_u32(binding.binding_point.element);
        hasher.add_u32(binding.binding_point.group);
        hasher.add_u32(binding.binding_point.index);
        hasher.add_u64(binding.view_key);
    }

    hasher.add_u64(static_cast<uint64_t>(in_structure.vertex_buffer_key_vec.size() ));

    for (const auto& vertex_buffer_key : in_structure.vertex_buffer_key_vec)
    {
        hasher.add_u64(vertex_buffer_key);
    }

    hasher.add_u64(in_structure.vertex_layout_key);

    return hasher.get_digest();
}

uint64_t Analysis::APIDumpAnalysis::KeyBuilder::compute_key_set_key(std::vector<uint64_t> in_key_vec)
{
    Hasher hasher;

    std::sort(in_key_vec.begin(),
              in_key_vec.end() );

    hasher.add_u64(static_cast<uint64_t>(in_key_vec.size() ));

    for (const auto& key : in_key_vec)
    {
        hasher.add_u64(key);
    }

    return hasher.get_digest();
}

uint64_t Analysis::APIDumpAnalysis::KeyBuilder::compute_pass_structure_key(const PassStructure& in_structure)
{
    Hasher hasher;

    hasher.add_bool(in_structure.clears_color);
    hasher.add_bool(in_structure.clears_depth);
    hasher.add_bool(in_structure.clears_stencil);
    hasher.add_u64 (static_cast<uint64_t>(in_structure.color_view_key_vec.size() ));

    for (const auto& color_view_key : in_structure.color_view_key_vec)
    {
        hasher.add_u64(color_view_key);
    }

    hasher.add_u64(static_cast<uint64_t>(in_structure.constant_signature_vec.size() ));

    for (const auto& signature : in_structure.constant_signature_vec)
    {
        hasher.add_u64(compute_constant_signature_key(signature) );
    }

    hasher.add_u64(in_structure.depth_stencil_view_key);
    hasher.add_u64(static_cast<uint64_t>(in_structure.draw_structure_key_vec.size() ));

    for (const auto& draw_structure_key : in_structure.draw_structure_key_vec)
    {
        hasher.add_u64(draw_structure_key);
    }

    hasher.add_f32(in_structure.viewport.height);
    hasher.add_f32(in_structure.viewport.max_depth);
    hasher.add_f32(in_structure.viewport.min_depth);
    hasher.add_f32(in_structure.viewport.width);
    hasher.add_f32(in_structure.viewport.x);
    hasher.add_f32(in_structure.viewport.y);

    return hasher.get_digest();
}
