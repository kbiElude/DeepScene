/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_analysis/query_commands.h"
#include "analysis/api_dump_analysis/timeline_builder.h"
#include "Common/macros.h"
#include <algorithm>
#include <string.h>

const char* Analysis::APIDumpAnalysis::QueryCommands::get_constant_block_kind_name(const ConstantBlockKind& in_kind)
{
    const char* result_ptr = "unknown";

    switch (in_kind)
    {
        case ConstantBlockKind::BUFFER:
        {
            result_ptr = "buffer";

            break;
        }

        case ConstantBlockKind::DEFAULT_UNIFORMS:
        {
            result_ptr = "default_uniforms";

            break;
        }

        case ConstantBlockKind::FIXED_FUNCTION_STATE:
        {
            result_ptr = "fixed_function_state";

            break;
        }

        case ConstantBlockKind::PUSH_CONSTANTS:
        {
            result_ptr = "push_constants";

            break;
        }

        default:
        {
            AI_FAIL_UNSUPPORTED(in_kind);

            break;
        }
    }

    return result_ptr;
}

const char* Analysis::APIDumpAnalysis::QueryCommands::get_constant_provenance_name(const ConstantProvenance& in_provenance)
{
    const char* result_ptr = "unknown";

    switch (in_provenance)
    {
        case ConstantProvenance::API_DEFINED:
        {
            result_ptr = "api_defined";

            break;
        }

        case ConstantProvenance::INFERRED:
        {
            result_ptr = "inferred";

            break;
        }

        default:
        {
            AI_FAIL_UNSUPPORTED(in_provenance);

            break;
        }
    }

    return result_ptr;
}

const char* Analysis::APIDumpAnalysis::QueryCommands::get_constant_semantic_name(const ConstantSemantic& in_semantic)
{
    const char* result_ptr = "unknown";

    switch (in_semantic)
    {
        case ConstantSemantic::MODEL_MATRIX:
        {
            result_ptr = "model_matrix";

            break;
        }

        case ConstantSemantic::MODEL_VIEW_MATRIX:
        {
            result_ptr = "model_view_matrix";

            break;
        }

        case ConstantSemantic::MODEL_VIEW_PROJECTION_MATRIX:
        {
            result_ptr = "model_view_projection_matrix";

            break;
        }

        case ConstantSemantic::PROJECTION_MATRIX:
        {
            result_ptr = "projection_matrix";

            break;
        }

        case ConstantSemantic::VIEW_MATRIX:
        {
            result_ptr = "view_matrix";

            break;
        }

        case ConstantSemantic::VIEW_PROJECTION_MATRIX:
        {
            result_ptr = "view_projection_matrix";

            break;
        }

        case ConstantSemantic::UNKNOWN:
        {
            break;
        }

        default:
        {
            AI_FAIL_UNSUPPORTED(in_semantic);

            break;
        }
    }

    return result_ptr;
}

Analysis::APIDumpAnalysis::QueryStatus Analysis::APIDumpAnalysis::QueryCommands::run_pass(const AnalysisResult&    in_analysis,
                                                                                          const QueryArguments&    in_arguments,
                                                                                          Utils::Json::JsonWriter* inout_writer_ptr,
                                                                                          std::string*             out_error_ptr)
{
    const auto&    model_ref = in_analysis.model;
    QueryStatus    result    = QueryStatus::BAD_ARGUMENTS;
    const Timeline timeline  = TimelineBuilder::build(in_analysis);

    if (in_arguments.has_pass_id == false)
    {
        *out_error_ptr = "pass needs a pass structure id";

        goto end;
    }

    if (in_arguments.pass_id >= model_ref.pass_structure_interner.get_n_structures() )
    {
        *out_error_ptr = "Pass structure "                                                     +
                         std::to_string(in_arguments.pass_id)                                  +
                         " does not exist (there are "                                         +
                         std::to_string(model_ref.pass_structure_interner.get_n_structures() ) + ")";
        result         = QueryStatus::NOT_FOUND;

        goto end;
    }

    {
        const uint32_t pass_id   = static_cast<uint32_t>(in_arguments.pass_id);
        const auto&    structure = model_ref.pass_structure_interner.get_structure(pass_id);
        const auto&    usage     = timeline.pass_usage_vec.at(pass_id);

        inout_writer_ptr->begin_object();

        inout_writer_ptr->write_key    ("pass_id");
        inout_writer_ptr->write_u64    (pass_id);
        inout_writer_ptr->write_key    ("key");
        inout_writer_ptr->write_hex_u64(structure.key);

        inout_writer_ptr->write_key("usage");
        inout_writer_ptr->begin_object();
        inout_writer_ptr->write_key("n_frames");
        inout_writer_ptr->write_u64(usage.n_frames);
        inout_writer_ptr->write_key("n_instances");
        inout_writer_ptr->write_u64(usage.n_instances);
        inout_writer_ptr->end_object();

        inout_writer_ptr->write_key   ("viewport");
        inout_writer_ptr->begin_object();
        inout_writer_ptr->write_key   ("x");
        inout_writer_ptr->write_double(structure.viewport.x);
        inout_writer_ptr->write_key   ("y");
        inout_writer_ptr->write_double(structure.viewport.y);
        inout_writer_ptr->write_key   ("width");
        inout_writer_ptr->write_double(structure.viewport.width);
        inout_writer_ptr->write_key   ("height");
        inout_writer_ptr->write_double(structure.viewport.height);
        inout_writer_ptr->write_key   ("min_depth");
        inout_writer_ptr->write_double(structure.viewport.min_depth);
        inout_writer_ptr->write_key   ("max_depth");
        inout_writer_ptr->write_double(structure.viewport.max_depth);
        inout_writer_ptr->end_object();

        inout_writer_ptr->write_key ("clears");
        inout_writer_ptr->begin_object();
        inout_writer_ptr->write_key ("color");
        inout_writer_ptr->write_bool(structure.clears_color);
        inout_writer_ptr->write_key ("depth");
        inout_writer_ptr->write_bool(structure.clears_depth);
        inout_writer_ptr->write_key ("stencil");
        inout_writer_ptr->write_bool(structure.clears_stencil);
        inout_writer_ptr->end_object();

        inout_writer_ptr->write_key("color_view_keys");
        inout_writer_ptr->begin_array();

        for (const auto& color_view_key : structure.color_view_key_vec)
        {
            inout_writer_ptr->write_hex_u64(color_view_key);
        }

        inout_writer_ptr->end_array();
        inout_writer_ptr->write_key    ("depth_stencil_view_key");
        inout_writer_ptr->write_hex_u64(structure.depth_stencil_view_key);

        inout_writer_ptr->write_key("constants");
        inout_writer_ptr->begin_array();

        for (size_t n_constant = 0;
                    n_constant < structure.constant_signature_vec.size();
                  ++n_constant)
        {
            const auto& signature   = structure.constant_signature_vec.at(n_constant);
            const auto  series_iter = model_ref.constant_series_map.find(ConstantSeriesKey(pass_id,
                                                                                           static_cast<uint32_t>(n_constant) ));

            inout_writer_ptr->begin_object();
            inout_writer_ptr->write_key    ("index");
            inout_writer_ptr->write_u64    (n_constant);
            inout_writer_ptr->write_key    ("block_kind");
            inout_writer_ptr->write_string (get_constant_block_kind_name(signature.block_kind) );
            inout_writer_ptr->write_key    ("block_key");
            inout_writer_ptr->write_hex_u64(signature.block_key);
            inout_writer_ptr->write_key    ("byte_offset");
            inout_writer_ptr->write_u64    (signature.byte_offset);
            inout_writer_ptr->write_key    ("byte_size");
            inout_writer_ptr->write_u64    (signature.byte_size);
            inout_writer_ptr->write_key    ("provenance");
            inout_writer_ptr->write_string (get_constant_provenance_name(signature.provenance) );
            inout_writer_ptr->write_key    ("semantic");
            inout_writer_ptr->write_string (get_constant_semantic_name(signature.semantic) );
            inout_writer_ptr->write_key    ("series");

            if (series_iter != model_ref.constant_series_map.end() )
            {
                write_series(in_analysis,
                             series_iter->second,
                             inout_writer_ptr);
            }
            else
            {
                inout_writer_ptr->write_null();
            }

            inout_writer_ptr->end_object();
        }

        inout_writer_ptr->end_array();

        inout_writer_ptr->write_key("draw_signatures");
        inout_writer_ptr->begin_array();

        for (const auto& draw_signature : structure.draw_signature_vec)
        {
            inout_writer_ptr->begin_object();
            inout_writer_ptr->write_key    ("signature_key");
            inout_writer_ptr->write_hex_u64(draw_signature.signature_key);
            inout_writer_ptr->write_key    ("structure_key");
            inout_writer_ptr->write_hex_u64(draw_signature.structure_key);
            inout_writer_ptr->end_object();
        }

        inout_writer_ptr->end_array();

        {
            const uint64_t n_draws = structure.draw_structure_key_vec.size();
            const uint64_t start   = std::min                             (in_arguments.offset,
                                                                           n_draws);

            inout_writer_ptr->write_key("draws");
            inout_writer_ptr->begin_object();

            write_page(n_draws,
                       in_arguments,
                       inout_writer_ptr);

            inout_writer_ptr->write_key("items");
            inout_writer_ptr->begin_array();

            for (uint64_t n_draw = start;
                          n_draw < get_page_end(n_draws,
                                                in_arguments);
                        ++n_draw)
            {
                const uint64_t key       = structure.draw_structure_key_vec.at(static_cast<size_t>(n_draw) );
                const auto     draw_iter = model_ref.draw_structure_map.find(key);

                if (draw_iter != model_ref.draw_structure_map.end() )
                {
                    write_draw_structure(key,
                                         draw_iter->second,
                                         inout_writer_ptr);
                }
                else
                {
                    inout_writer_ptr->begin_object();
                    inout_writer_ptr->write_key    ("key");
                    inout_writer_ptr->write_hex_u64(key);
                    inout_writer_ptr->end_object();
                }
            }

            inout_writer_ptr->end_array();
            inout_writer_ptr->end_object();
        }

        inout_writer_ptr->end_object();
    }

    result = QueryStatus::OK;

end:
    return result;
}

void Analysis::APIDumpAnalysis::QueryCommands::write_draw_structure(const uint64_t&          in_key,
                                                                    const DrawStructure&     in_structure,
                                                                    Utils::Json::JsonWriter* inout_writer_ptr)
{
    inout_writer_ptr->begin_object();

    inout_writer_ptr->write_key    ("key");
    inout_writer_ptr->write_hex_u64(in_key);
    inout_writer_ptr->write_key    ("topology");
    inout_writer_ptr->write_string (get_primitive_topology_name(in_structure.primitive_topology) );
    inout_writer_ptr->write_key    ("is_indexed");
    inout_writer_ptr->write_bool   (in_structure.is_indexed);
    inout_writer_ptr->write_key    ("is_instanced");
    inout_writer_ptr->write_bool   (in_structure.is_instanced);
    inout_writer_ptr->write_key    ("index_buffer_key");
    inout_writer_ptr->write_hex_u64(in_structure.index_buffer_key);
    inout_writer_ptr->write_key    ("vertex_buffer_keys");
    inout_writer_ptr->begin_array();

    for (const auto& vertex_buffer_key : in_structure.vertex_buffer_key_vec)
    {
        inout_writer_ptr->write_hex_u64(vertex_buffer_key);
    }

    inout_writer_ptr->end_array();
    inout_writer_ptr->write_key    ("vertex_layout_key");
    inout_writer_ptr->write_hex_u64(in_structure.vertex_layout_key);
    inout_writer_ptr->write_key    ("shader_keys");
    inout_writer_ptr->begin_array();

    for (const auto& shader_key : in_structure.shader_key_vec)
    {
        inout_writer_ptr->write_hex_u64(shader_key);
    }

    inout_writer_ptr->end_array();
    inout_writer_ptr->write_key    ("state_key");
    inout_writer_ptr->write_hex_u64(in_structure.state_key);
    inout_writer_ptr->write_key    ("texture_bindings");
    inout_writer_ptr->begin_array();

    for (const auto& binding : in_structure.texture_binding_key_vec)
    {
        inout_writer_ptr->begin_object();
        inout_writer_ptr->write_key    ("element");
        inout_writer_ptr->write_u64    (binding.binding_point.element);
        inout_writer_ptr->write_key    ("group");
        inout_writer_ptr->write_u64    (binding.binding_point.group);
        inout_writer_ptr->write_key    ("index");
        inout_writer_ptr->write_u64    (binding.binding_point.index);
        inout_writer_ptr->write_key    ("view_key");
        inout_writer_ptr->write_hex_u64(binding.view_key);
        inout_writer_ptr->end_object();
    }

    inout_writer_ptr->end_array();
    inout_writer_ptr->end_object();
}

void Analysis::APIDumpAnalysis::QueryCommands::write_sample(const AnalysisResult&          in_analysis,
                                                            const ValueTimeSeries::Sample& in_sample,
                                                            Utils::Json::JsonWriter*       inout_writer_ptr)
{
    constexpr size_t MAX_F32_N_BYTES = 256; /* longer values are given in hex only   */

    inout_writer_ptr->begin_object();
    inout_writer_ptr->write_key   ("seconds");
    inout_writer_ptr->write_double(get_seconds(in_analysis,
                                               in_sample.timestamp_qpc) );
    inout_writer_ptr->write_key   ("value_hex");
    inout_writer_ptr->write_string(get_hex_string(in_sample.value_u8_vec) );

    if (in_sample.value_u8_vec.size() %  sizeof(float) == 0 &&
        in_sample.value_u8_vec.size() <= MAX_F32_N_BYTES)
    {
        inout_writer_ptr->write_key("value_f32");
        inout_writer_ptr->begin_inline_array();

        for (size_t n_value = 0;
                    n_value < in_sample.value_u8_vec.size() / sizeof(float);
                  ++n_value)
        {
            float value = 0.0f;

            ::memcpy(&value,
                     in_sample.value_u8_vec.data() + n_value * sizeof(float),
                     sizeof(value) );

            inout_writer_ptr->write_float(value);
        }

        inout_writer_ptr->end_array();
    }

    inout_writer_ptr->end_object();
}

void Analysis::APIDumpAnalysis::QueryCommands::write_series(const AnalysisResult&    in_analysis,
                                                            const ValueTimeSeries&   in_series,
                                                            Utils::Json::JsonWriter* inout_writer_ptr)
{
    const auto& sample_vec = in_series.get_sample_vec();

    inout_writer_ptr->begin_object();
    inout_writer_ptr->write_key("n_samples");
    inout_writer_ptr->write_u64(sample_vec.size() );
    inout_writer_ptr->write_key("n_updates");
    inout_writer_ptr->write_u64(in_series.get_n_updates() );

    if (sample_vec.empty() == false)
    {
        inout_writer_ptr->write_key("first");

        write_sample(in_analysis,
                     sample_vec.front(),
                     inout_writer_ptr);

        inout_writer_ptr->write_key("last");

        write_sample(in_analysis,
                     sample_vec.back(),
                     inout_writer_ptr);
    }

    inout_writer_ptr->end_object();
}
