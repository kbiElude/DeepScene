/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_analysis/types.h"
#include "Common/macros.h"

const char* Analysis::APIDumpAnalysis::get_resolution_hazard_name(const ResolutionHazard& in_hazard)
{
    const char* result_ptr = "unknown";

    switch (in_hazard)
    {
        case ResolutionHazard::CLEARS_WITH_RECTANGLES:
        {
            result_ptr = "clears_with_rectangles";

            break;
        }

        case ResolutionHazard::COPIES_WITH_RECTANGLES:
        {
            result_ptr = "copies_with_rectangles";

            break;
        }

        case ResolutionHazard::POINT_SIZE_STATES:
        {
            result_ptr = "point_size_states";

            break;
        }

        case ResolutionHazard::SCREEN_SPACE_VERTICES:
        {
            result_ptr = "screen_space_vertices";

            break;
        }

        case ResolutionHazard::SHADER_CREATIONS:
        {
            result_ptr = "shader_creations";

            break;
        }

        case ResolutionHazard::SIZE_QUERIES:
        {
            result_ptr = "size_queries";

            break;
        }

        default:
        {
            AI_FAIL_UNSUPPORTED(in_hazard);

            break;
        }
    }

    return result_ptr;
}

uint32_t Analysis::APIDumpAnalysis::get_resource_id_value(const ResourceId& in_id)
{
    return static_cast<uint32_t>(in_id);
}

Analysis::APIDumpAnalysis::ResourceId Analysis::APIDumpAnalysis::make_resource_id(const uint32_t& in_value)
{
    return static_cast<ResourceId>(in_value);
}

bool Analysis::APIDumpAnalysis::operator==(const BindingPoint& in_a,
                                           const BindingPoint& in_b)
{
    return (in_a.element == in_b.element &&
            in_a.group   == in_b.group   &&
            in_a.index   == in_b.index);
}

bool Analysis::APIDumpAnalysis::operator==(const ClearOperation& in_a,
                                           const ClearOperation& in_b)
{
    return (in_a.clears_color           == in_b.clears_color           &&
            in_a.clears_depth           == in_b.clears_depth           &&
            in_a.clears_stencil         == in_b.clears_stencil         &&
            in_a.color_fp32vec4         == in_b.color_fp32vec4         &&
            in_a.depth                  == in_b.depth                  &&
            in_a.has_rectangles         == in_b.has_rectangles         &&
            in_a.is_limited_to_viewport == in_b.is_limited_to_viewport &&
            in_a.stencil                == in_b.stencil);
}

bool Analysis::APIDumpAnalysis::operator==(const ConstantLocation& in_a,
                                           const ConstantLocation& in_b)
{
    return (in_a.block_id    == in_b.block_id    &&
            in_a.block_kind  == in_b.block_kind  &&
            in_a.byte_offset == in_b.byte_offset &&
            in_a.byte_size   == in_b.byte_size);
}

bool Analysis::APIDumpAnalysis::operator==(const ConstantSignature& in_a,
                                           const ConstantSignature& in_b)
{
    return (in_a.block_key   == in_b.block_key   &&
            in_a.block_kind  == in_b.block_kind  &&
            in_a.byte_offset == in_b.byte_offset &&
            in_a.byte_size   == in_b.byte_size   &&
            in_a.provenance  == in_b.provenance  &&
            in_a.semantic    == in_b.semantic);
}

bool Analysis::APIDumpAnalysis::operator==(const ConstantUpdate& in_a,
                                           const ConstantUpdate& in_b)
{
    return (in_a.location     == in_b.location   &&
            in_a.provenance   == in_b.provenance &&
            in_a.semantic     == in_b.semantic   &&
            in_a.value_u8_vec == in_b.value_u8_vec);
}

bool Analysis::APIDumpAnalysis::operator==(const DataReference& in_a,
                                           const DataReference& in_b)
{
    return (in_a.content_hash  == in_b.content_hash  &&
            in_a.data_chunk_id == in_b.data_chunk_id &&
            in_a.n_bytes       == in_b.n_bytes);
}

bool Analysis::APIDumpAnalysis::operator==(const DrawDescription& in_a,
                                           const DrawDescription& in_b)
{
    return (in_a.index_buffer_id         == in_b.index_buffer_id         &&
            in_a.instance_count          == in_b.instance_count          &&
            in_a.is_indexed              == in_b.is_indexed              &&
            in_a.primitive_topology      == in_b.primitive_topology      &&
            in_a.reads_color_attachments == in_b.reads_color_attachments &&
            in_a.reads_depth_stencil     == in_b.reads_depth_stencil     &&
            in_a.shader_id_vec           == in_b.shader_id_vec           &&
            in_a.state_key               == in_b.state_key               &&
            in_a.texture_binding_vec     == in_b.texture_binding_vec     &&
            in_a.vertex_buffer_id_vec    == in_b.vertex_buffer_id_vec    &&
            in_a.vertex_count            == in_b.vertex_count            &&
            in_a.vertex_layout_key       == in_b.vertex_layout_key);
}

bool Analysis::APIDumpAnalysis::operator==(const DrawSignature& in_a,
                                           const DrawSignature& in_b)
{
    return (in_a.signature_key == in_b.signature_key &&
            in_a.structure_key == in_b.structure_key);
}

bool Analysis::APIDumpAnalysis::operator==(const DrawStructure& in_a,
                                           const DrawStructure& in_b)
{
    return (in_a.index_buffer_key        == in_b.index_buffer_key        &&
            in_a.is_indexed              == in_b.is_indexed              &&
            in_a.is_instanced            == in_b.is_instanced            &&
            in_a.primitive_topology      == in_b.primitive_topology      &&
            in_a.shader_key_vec          == in_b.shader_key_vec          &&
            in_a.state_key               == in_b.state_key               &&
            in_a.texture_binding_key_vec == in_b.texture_binding_key_vec &&
            in_a.vertex_buffer_key_vec   == in_b.vertex_buffer_key_vec   &&
            in_a.vertex_layout_key       == in_b.vertex_layout_key);
}

bool Analysis::APIDumpAnalysis::operator==(const Format& in_a,
                                           const Format& in_b)
{
    return (in_a.format_class == in_b.format_class &&
            in_a.raw_value    == in_b.raw_value);
}

bool Analysis::APIDumpAnalysis::operator==(const FrameShape& in_a,
                                           const FrameShape& in_b)
{
    return (in_a.pass_structure_id_vec == in_b.pass_structure_id_vec);
}

bool Analysis::APIDumpAnalysis::operator==(const PassStructure& in_a,
                                           const PassStructure& in_b)
{
    return (in_a.clears_color            == in_b.clears_color            &&
            in_a.clears_depth            == in_b.clears_depth            &&
            in_a.clears_stencil          == in_b.clears_stencil          &&
            in_a.color_view_key_vec      == in_b.color_view_key_vec      &&
            in_a.constant_signature_vec  == in_b.constant_signature_vec  &&
            in_a.depth_stencil_view_key  == in_b.depth_stencil_view_key  &&
            in_a.draw_signature_vec      == in_b.draw_signature_vec      &&
            in_a.draw_structure_key_vec  == in_b.draw_structure_key_vec  &&
            in_a.key                     == in_b.key                     &&
            in_a.viewport                == in_b.viewport);
}

bool Analysis::APIDumpAnalysis::operator==(const ResourceDescription& in_a,
                                           const ResourceDescription& in_b)
{
    return (in_a.api_flags            == in_b.api_flags            &&
            in_a.content_hash         == in_b.content_hash         &&
            in_a.depth_or_layer_count == in_b.depth_or_layer_count &&
            in_a.format               == in_b.format               &&
            in_a.has_content_hash     == in_b.has_content_hash     &&
            in_a.height               == in_b.height               &&
            in_a.is_depth_stencil     == in_b.is_depth_stencil     &&
            in_a.is_dynamic           == in_b.is_dynamic           &&
            in_a.is_render_target     == in_b.is_render_target     &&
            in_a.kind                 == in_b.kind                 &&
            in_a.mip_level_count      == in_b.mip_level_count      &&
            in_a.sample_count         == in_b.sample_count         &&
            in_a.shader_stage         == in_b.shader_stage         &&
            in_a.size_in_bytes        == in_b.size_in_bytes        &&
            in_a.width                == in_b.width);
}

bool Analysis::APIDumpAnalysis::operator==(const ResourceView& in_a,
                                           const ResourceView& in_b)
{
    return (in_a.layer       == in_b.layer     &&
            in_a.mip_level   == in_b.mip_level &&
            in_a.resource_id == in_b.resource_id);
}

bool Analysis::APIDumpAnalysis::operator==(const TextureBinding& in_a,
                                           const TextureBinding& in_b)
{
    return (in_a.binding_point == in_b.binding_point &&
            in_a.view          == in_b.view);
}

bool Analysis::APIDumpAnalysis::operator==(const TextureBindingKey& in_a,
                                           const TextureBindingKey& in_b)
{
    return (in_a.binding_point == in_b.binding_point &&
            in_a.view_key      == in_b.view_key);
}

bool Analysis::APIDumpAnalysis::operator==(const Viewport& in_a,
                                           const Viewport& in_b)
{
    return (in_a.height    == in_b.height    &&
            in_a.max_depth == in_b.max_depth &&
            in_a.min_depth == in_b.min_depth &&
            in_a.width     == in_b.width     &&
            in_a.x         == in_b.x         &&
            in_a.y         == in_b.y);
}
