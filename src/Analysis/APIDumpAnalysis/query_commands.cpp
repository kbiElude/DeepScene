/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "Analysis/APIDumpAnalysis/depth_histogram.h"
#include "Analysis/APIDumpAnalysis/key_builder.h"
#include "Analysis/APIDumpAnalysis/model_builder.h"
#include "Analysis/APIDumpAnalysis/passes_format.h"
#include "Analysis/APIDumpAnalysis/resource_access_tracker.h"
#include "Analysis/APIDumpAnalysis/query_commands.h"
#include "Analysis/APIDumpAnalysis/resource_identity.h"
#include "Analysis/APIDumpAnalysis/timeline_builder.h"
#include "Common/macros.h"
#include <algorithm>
#include <set>
#include <string.h>

const std::vector<std::string>& Analysis::APIDumpAnalysis::QueryCommands::get_command_names()
{
    static const std::vector<std::string> result_vec =
    {
        "depth",
        "frame",
        "info",
        "overlays",
        "pass",
        "pipelines",
        "resources",
        "timeline"
    };

    return result_vec;
}

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

bool Analysis::APIDumpAnalysis::QueryCommands::get_first_frame_index(const AnalysisResult& in_analysis,
                                                                     const uint32_t&       in_pass_id,
                                                                     uint64_t*             out_frame_index_ptr)
{
    const auto& model_ref = in_analysis.model;
    bool        result    = false;

    /* The frames are in order, and a frame has the passes of its shape. */
    for (const auto& frame_record : model_ref.frame_record_vec)
    {
        const auto& shape = model_ref.frame_shape_interner.get_structure(frame_record.shape_id);

        if (std::binary_search(shape.pass_structure_id_vec.begin(),
                               shape.pass_structure_id_vec.end  (),
                               in_pass_id) )
        {
            *out_frame_index_ptr = frame_record.frame_index;
            result               = true;

            break;
        }
    }

    return result;
}

std::string Analysis::APIDumpAnalysis::QueryCommands::get_hex_string(const std::vector<uint8_t>& in_byte_vec)
{
    static const char DIGIT_ARRAY[] = "0123456789abcdef";

    constexpr size_t N_BITS_PER_DIGIT = 4;
    constexpr size_t DIGIT_MASK        = 0x0f;

    std::string result;

    for (const auto& byte : in_byte_vec)
    {
        result.push_back(DIGIT_ARRAY[(byte >> N_BITS_PER_DIGIT) & DIGIT_MASK]);
        result.push_back(DIGIT_ARRAY[byte & DIGIT_MASK]);
    }

    return result;
}

uint64_t Analysis::APIDumpAnalysis::QueryCommands::get_page_end(const uint64_t&       in_total,
                                                                const QueryArguments& in_arguments)
{
    const uint64_t n_items_left = in_total - std::min(in_arguments.offset,
                                                      in_total);
    uint64_t       result       = in_total;

    if (in_arguments.limit != 0 &&
        in_arguments.limit <  n_items_left)
    {
        result = in_arguments.offset + in_arguments.limit;
    }

    return result;
}

Analysis::APIDumpAnalysis::PassDepthSpread Analysis::APIDumpAnalysis::QueryCommands::get_pass_depth_spread(const AnalysisResult& in_analysis,
                                                                                                           const uint32_t&       in_pass_id)
{
    constexpr double FRACTION_FAR    = 0.9;
    constexpr double FRACTION_MEDIAN = 0.5;
    constexpr double FRACTION_NEAR   = 0.1;
    constexpr float  MAX_FLAT_RATIO  = 1.25f; /* a range of the histogram is an eighth of a doubling wide: vertices of one depth are in one or two of them */

    const auto&     model_ref = in_analysis.model;
    DepthHistogram  origin_histogram;
    PassDepthSpread result;
    DepthHistogram  surface_histogram;

    for (const auto& histogram_pair : model_ref.depth_histogram_map)
    {
        if (histogram_pair.first.second == in_pass_id)
        {
            origin_histogram.merge(histogram_pair.second);
        }
    }

    for (const auto& histogram_pair : model_ref.surface_depth_histogram_map)
    {
        if (histogram_pair.first.second == in_pass_id)
        {
            surface_histogram.merge(histogram_pair.second);
        }
    }

    /* The vertices of the draws say more than the origins of their objects, which may be anywhere. */
    const auto& histogram = (surface_histogram.is_empty() ) ? origin_histogram
                                                            : surface_histogram;

    if (histogram.is_empty() == false)
    {
        result.depth_far    = histogram.get_depth_at_fraction(FRACTION_FAR);
        result.depth_median = histogram.get_depth_at_fraction(FRACTION_MEDIAN);
        result.depth_near   = histogram.get_depth_at_fraction(FRACTION_NEAR);
        result.has_depth    = (result.depth_near > 0.0f);
        result.n_vertices   = histogram.get_total_weight();

        if (result.has_depth)
        {
            result.far_to_near_ratio = result.depth_far / result.depth_near;
            result.is_flat           = (result.far_to_near_ratio <= MAX_FLAT_RATIO);
        }
    }

    return result;
}

const char* Analysis::APIDumpAnalysis::QueryCommands::get_primitive_topology_name(const PrimitiveTopology& in_topology)
{
    const char* result_ptr = "unknown";

    switch (in_topology)
    {
        case PrimitiveTopology::LINE_LIST:
        {
            result_ptr = "line_list";

            break;
        }

        case PrimitiveTopology::LINE_STRIP:
        {
            result_ptr = "line_strip";

            break;
        }

        case PrimitiveTopology::POINT_LIST:
        {
            result_ptr = "point_list";

            break;
        }

        case PrimitiveTopology::TRIANGLE_FAN:
        {
            result_ptr = "triangle_fan";

            break;
        }

        case PrimitiveTopology::TRIANGLE_LIST:
        {
            result_ptr = "triangle_list";

            break;
        }

        case PrimitiveTopology::TRIANGLE_STRIP:
        {
            result_ptr = "triangle_strip";

            break;
        }

        case PrimitiveTopology::UNKNOWN:
        {
            break;
        }

        default:
        {
            AI_FAIL_UNSUPPORTED(in_topology);

            break;
        }
    }

    return result_ptr;
}

const char* Analysis::APIDumpAnalysis::QueryCommands::get_resource_access_class_name(const ResourceAccessClass& in_class)
{
    const char* result_ptr = "unknown";

    switch (in_class)
    {
        case ResourceAccessClass::FRAME_LOCAL:
        {
            result_ptr = "frame_local";

            break;
        }

        case ResourceAccessClass::LOOP_CARRIED:
        {
            result_ptr = "loop_carried";

            break;
        }

        case ResourceAccessClass::STATIC:
        {
            result_ptr = "static";

            break;
        }

        case ResourceAccessClass::UNREAD:
        {
            result_ptr = "unread";

            break;
        }

        case ResourceAccessClass::UNUSED:
        {
            result_ptr = "unused";

            break;
        }

        default:
        {
            AI_FAIL_UNSUPPORTED(in_class);

            break;
        }
    }

    return result_ptr;
}

const char* Analysis::APIDumpAnalysis::QueryCommands::get_resource_kind_name(const ResourceKind& in_kind)
{
    const char* result_ptr = "unknown";

    switch (in_kind)
    {
        case ResourceKind::BUFFER_INDEX:
        {
            result_ptr = "index_buffer";

            break;
        }

        case ResourceKind::BUFFER_VERTEX:
        {
            result_ptr = "vertex_buffer";

            break;
        }

        case ResourceKind::SHADER:
        {
            result_ptr = "shader";

            break;
        }

        case ResourceKind::SWAP_CHAIN_IMAGE:
        {
            result_ptr = "swap_chain_image";

            break;
        }

        case ResourceKind::TEXTURE_2D:
        {
            result_ptr = "texture_2d";

            break;
        }

        case ResourceKind::TEXTURE_3D:
        {
            result_ptr = "texture_3d";

            break;
        }

        case ResourceKind::TEXTURE_CUBE:
        {
            result_ptr = "texture_cube";

            break;
        }

        case ResourceKind::UNKNOWN:
        {
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

double Analysis::APIDumpAnalysis::QueryCommands::get_seconds(const AnalysisResult& in_analysis,
                                                             const uint64_t&       in_timestamp_qpc)
{
    double result = 0.0;

    if (in_analysis.summary.qpc_frequency != 0 &&
        in_timestamp_qpc                  >= in_analysis.summary.first_timestamp_qpc)
    {
        result = static_cast<double>(in_timestamp_qpc - in_analysis.summary.first_timestamp_qpc) /
                 static_cast<double>(in_analysis.summary.qpc_frequency);
    }

    return result;
}


std::vector<uint32_t> Analysis::APIDumpAnalysis::QueryCommands::get_static_camera_pass_id_vec(const AnalysisResult& in_analysis)
{
    const auto&           model_ref = in_analysis.model;
    std::vector<uint32_t> result_vec;

    /* The camera of a pass which never changes is the sign of a pass that is drawn in screen space. */
    for (uint32_t n_pass = 0;
                  n_pass < model_ref.pass_structure_interner.get_n_structures();
                ++n_pass)
    {
        const auto& structure         = model_ref.pass_structure_interner.get_structure(n_pass);
        bool        has_camera        = false;
        bool        is_camera_changed = false;

        for (size_t n_constant = 0;
                    n_constant < structure.constant_signature_vec.size();
                  ++n_constant)
        {
            const auto& signature   = structure.constant_signature_vec.at(n_constant);
            const auto  series_iter = model_ref.constant_series_map.find (ConstantSeriesKey(n_pass,
                                                                                            static_cast<uint32_t>(n_constant) ));

            if ((signature.semantic == ConstantSemantic::VIEW_MATRIX             ||
                 signature.semantic == ConstantSemantic::VIEW_PROJECTION_MATRIX) &&
                 series_iter        != model_ref.constant_series_map.end() )
            {
                has_camera        = true;
                is_camera_changed = is_camera_changed || (series_iter->second.get_sample_vec().size() > 1);
            }
        }

        if (has_camera                 &&
            is_camera_changed == false)
        {
            result_vec.push_back(n_pass);
        }
    }

    return result_vec;
}

const char* Analysis::APIDumpAnalysis::QueryCommands::get_transfer_kind_name(const TransferKind& in_kind)
{
    const char* result_ptr = "unknown";

    switch (in_kind)
    {
        case TransferKind::COPY:
        {
            result_ptr = "copy";

            break;
        }

        case TransferKind::UPLOAD:
        {
            result_ptr = "upload";

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

bool Analysis::APIDumpAnalysis::QueryCommands::parse_resource_access_class(const std::string&   in_name,
                                                                           ResourceAccessClass* out_class_ptr)
{
    static const ResourceAccessClass CLASS_ARRAY[] =
    {
        ResourceAccessClass::FRAME_LOCAL,
        ResourceAccessClass::LOOP_CARRIED,
        ResourceAccessClass::STATIC,
        ResourceAccessClass::UNREAD,
        ResourceAccessClass::UNUSED
    };

    bool result = false;

    for (const auto& access_class : CLASS_ARRAY)
    {
        if (in_name == get_resource_access_class_name(access_class) )
        {
            *out_class_ptr = access_class;

            result = true;
        }
    }

    return result;
}

Analysis::APIDumpAnalysis::QueryStatus Analysis::APIDumpAnalysis::QueryCommands::run(const std::string&    in_command,
                                                                                     const AnalysisResult& in_analysis,
                                                                                     const QueryArguments& in_arguments,
                                                                                     JsonWriter*           inout_writer_ptr,
                                                                                     std::string*          out_error_ptr)
{
    QueryStatus result = QueryStatus::BAD_ARGUMENTS;

    if (in_command == "depth")
    {
        result = run_depth(in_analysis,
                           inout_writer_ptr);
    }
    else
    if (in_command == "frame")
    {
        result = run_frame(in_analysis,
                           in_arguments,
                           inout_writer_ptr,
                           out_error_ptr);
    }
    else
    if (in_command == "info")
    {
        result = run_info(in_analysis,
                          inout_writer_ptr);
    }
    else
    if (in_command == "overlays")
    {
        result = run_overlays(in_analysis,
                              inout_writer_ptr);
    }
    else
    if (in_command == "pass")
    {
        result = run_pass(in_analysis,
                          in_arguments,
                          inout_writer_ptr,
                          out_error_ptr);
    }
    else
    if (in_command == "pipelines")
    {
        result = run_pipelines(in_analysis,
                               in_arguments,
                               inout_writer_ptr);
    }
    else
    if (in_command == "resources")
    {
        result = run_resources(in_analysis,
                               in_arguments,
                               inout_writer_ptr);
    }
    else
    if (in_command == "timeline")
    {
        result = run_timeline(in_analysis,
                              in_arguments,
                              inout_writer_ptr);
    }
    else
    {
        *out_error_ptr = "Unknown command " + in_command;
    }

    return result;
}

Analysis::APIDumpAnalysis::QueryStatus Analysis::APIDumpAnalysis::QueryCommands::run_frame(const AnalysisResult& in_analysis,
                                                                                           const QueryArguments& in_arguments,
                                                                                           JsonWriter*           inout_writer_ptr,
                                                                                           std::string*          out_error_ptr)
{
    const RepresentativeStore::Entry*   entry_ptr     = nullptr;
    RepresentativeStore::EntryPtrVector entry_ptr_vec;
    uint64_t                            frame_index   = 0;
    const auto&                         model_ref     = in_analysis.model;
    QueryStatus                         result        = QueryStatus::BAD_ARGUMENTS;

    if (in_arguments.has_shape_id)
    {
        if (in_arguments.shape_id >= model_ref.frame_shape_interner.get_n_structures() )
        {
            *out_error_ptr = "Frame shape "                                                     +
                             std::to_string(in_arguments.shape_id)                              +
                             " does not exist (there are "                                      +
                             std::to_string(model_ref.frame_shape_interner.get_n_structures() ) +
                             ")";
            result         = QueryStatus::NOT_FOUND;

            goto end;
        }

        entry_ptr_vec = model_ref.representative_store.get_entry_ptr_vec(static_cast<uint32_t>(in_arguments.shape_id) );

        if (entry_ptr_vec.empty() )
        {
            *out_error_ptr = "Frame shape " + std::to_string(in_arguments.shape_id) + " has no representative frame";
            result         = QueryStatus::NOT_FOUND;

            goto end;
        }

        entry_ptr = entry_ptr_vec.front();

        for (const auto& candidate_ptr : entry_ptr_vec)
        {
            bool is_better = false;

            switch (in_arguments.which)
            {
                case RepresentativeChoice::HEAVIEST:
                {
                    is_better = (candidate_ptr->weight > entry_ptr->weight);

                    break;
                }

                case RepresentativeChoice::LIGHTEST:
                {
                    is_better = (candidate_ptr->weight < entry_ptr->weight);

                    break;
                }

                case RepresentativeChoice::FIRST:
                {
                    break;
                }

                default:
                {
                    AI_FAIL_UNSUPPORTED(in_arguments.which);

                    break;
                }
            }

            if (is_better)
            {
                entry_ptr = candidate_ptr;
            }
        }

        frame_index = entry_ptr->frame_index;
    }
    else
    if (in_arguments.has_frame_index)
    {
        if (in_arguments.frame_index >= model_ref.frame_record_vec.size() )
        {
            *out_error_ptr = "Frame "                                           +
                             std::to_string(in_arguments.frame_index)           +
                             " does not exist (there are "                      +
                             std::to_string(model_ref.frame_record_vec.size() ) +
                             ")";
            result         = QueryStatus::NOT_FOUND;

            goto end;
        }

        frame_index   = in_arguments.frame_index;
        entry_ptr_vec = model_ref.representative_store.get_entry_ptr_vec(model_ref.frame_record_vec.at(static_cast<size_t>(frame_index) ).shape_id);

        for (const auto& candidate_ptr : entry_ptr_vec)
        {
            if (candidate_ptr->frame_index == frame_index)
            {
                entry_ptr = candidate_ptr;
            }
        }
    }
    else
    {
        *out_error_ptr = "frame needs a frame index, or --shape";

        goto end;
    }

    inout_writer_ptr->begin_object();

    if (in_arguments.has_shape_id)
    {
        const char* which_ptr = "first";

        if (in_arguments.which == RepresentativeChoice::HEAVIEST)
        {
            which_ptr = "heaviest";
        }
        else
        if (in_arguments.which == RepresentativeChoice::LIGHTEST)
        {
            which_ptr = "lightest";
        }

        inout_writer_ptr->write_key   ("shape_id");
        inout_writer_ptr->write_u64   (in_arguments.shape_id);
        inout_writer_ptr->write_key   ("which");
        inout_writer_ptr->write_string(which_ptr);
        inout_writer_ptr->write_key   ("representatives");
        inout_writer_ptr->begin_array();

        for (const auto& candidate_ptr : entry_ptr_vec)
        {
            inout_writer_ptr->begin_object();
            inout_writer_ptr->write_key("frame_index");
            inout_writer_ptr->write_u64(candidate_ptr->frame_index);
            inout_writer_ptr->write_key("n_draws");
            inout_writer_ptr->write_u64(candidate_ptr->weight);
            inout_writer_ptr->end_object();
        }

        inout_writer_ptr->end_array();
    }

    inout_writer_ptr->write_key("frame");

    write_frame(in_analysis,
                frame_index,
                entry_ptr,
                inout_writer_ptr);

    inout_writer_ptr->end_object();

    result = QueryStatus::OK;

end:
    return result;
}

Analysis::APIDumpAnalysis::QueryStatus Analysis::APIDumpAnalysis::QueryCommands::run_depth(const AnalysisResult& in_analysis,
                                                                                           JsonWriter*           inout_writer_ptr)
{
    constexpr double FRACTION_FAR    = 0.9;
    constexpr double FRACTION_MEDIAN = 0.5;
    constexpr double FRACTION_NEAR   = 0.1;

    const auto& model_ref = in_analysis.model;

    inout_writer_ptr->begin_object();
    inout_writer_ptr->write_key   ("passes");
    inout_writer_ptr->begin_array();

    /* A pass whose objects have their origins behind the camera still has surfaces in front of it. */
    std::set<DepthHistogramKey> key_set;
    const DepthHistogram        no_histogram;

    for (const auto& histogram_pair : model_ref.depth_histogram_map)
    {
        key_set.insert(histogram_pair.first);
    }

    for (const auto& histogram_pair : model_ref.surface_depth_histogram_map)
    {
        key_set.insert(histogram_pair.first);
    }

    for (const auto& key : key_set)
    {
        const auto  origin_iterator = model_ref.depth_histogram_map.find(key);
        const auto& origin          = (origin_iterator != model_ref.depth_histogram_map.end() ) ? origin_iterator->second
                                                                                                  : no_histogram;
        const auto& structure       = model_ref.pass_structure_interner.get_structure(key.second);

        inout_writer_ptr->begin_object();
        inout_writer_ptr->write_key    ("shape_id");
        inout_writer_ptr->write_u64    (key.first);
        inout_writer_ptr->write_key    ("shape_key");
        inout_writer_ptr->write_hex_u64(ModelBuilder::compute_shape_key(model_ref,
                                                                        key.first) );
        inout_writer_ptr->write_key    ("pass_id");
        inout_writer_ptr->write_u64    (key.second);
        inout_writer_ptr->write_key    ("pass_key");
        inout_writer_ptr->write_hex_u64(structure.key);
        inout_writer_ptr->write_key    ("n_vertices");
        inout_writer_ptr->write_u64    (origin.get_total_weight() );
        inout_writer_ptr->write_key    ("depth_near");
        inout_writer_ptr->write_float  (origin.get_depth_at_fraction(FRACTION_NEAR) );
        inout_writer_ptr->write_key    ("depth_median");
        inout_writer_ptr->write_float  (origin.get_depth_at_fraction(FRACTION_MEDIAN) );
        inout_writer_ptr->write_key    ("depth_far");
        inout_writer_ptr->write_float  (origin.get_depth_at_fraction(FRACTION_FAR) );

        /* Of the vertices that the draws draw, and not of the origins of their objects, if the analysis could tell them. */
        const auto surface_iterator = model_ref.surface_depth_histogram_map.find(key);

        if (surface_iterator != model_ref.surface_depth_histogram_map.end() )
        {
            inout_writer_ptr->write_key  ("surface_n_vertices");
            inout_writer_ptr->write_u64  (surface_iterator->second.get_total_weight() );
            inout_writer_ptr->write_key  ("surface_depth_nearest");
            inout_writer_ptr->write_float(surface_iterator->second.get_depth_at_fraction(0.0005) );
            inout_writer_ptr->write_key  ("surface_depth_near");
            inout_writer_ptr->write_float(surface_iterator->second.get_depth_at_fraction(FRACTION_NEAR) );
            inout_writer_ptr->write_key  ("surface_depth_median");
            inout_writer_ptr->write_float(surface_iterator->second.get_depth_at_fraction(FRACTION_MEDIAN) );
            inout_writer_ptr->write_key  ("surface_depth_far");
            inout_writer_ptr->write_float(surface_iterator->second.get_depth_at_fraction(FRACTION_FAR) );
        }

        inout_writer_ptr->end_object();
    }

    inout_writer_ptr->end_array();
    inout_writer_ptr->end_object();

    return QueryStatus::OK;
}

Analysis::APIDumpAnalysis::QueryStatus Analysis::APIDumpAnalysis::QueryCommands::run_info(const AnalysisResult& in_analysis,
                                                                                          JsonWriter*           inout_writer_ptr)
{
    const auto&    model_ref = in_analysis.model;
    uint64_t       n_draws   = 0;
    const Timeline timeline  = TimelineBuilder::build(in_analysis);

    for (const auto& frame : model_ref.frame_record_vec)
    {
        n_draws += frame.n_draws;
    }

    inout_writer_ptr->begin_object();

    inout_writer_ptr->write_key   ("workload");
    inout_writer_ptr->begin_object();
    inout_writer_ptr->write_key   ("filename");
    inout_writer_ptr->write_string(in_analysis.workload_reference.filename);
    inout_writer_ptr->write_key   ("header_hash");
    inout_writer_ptr->write_hex_u64(in_analysis.workload_reference.header_hash);
    inout_writer_ptr->write_key   ("n_bytes");
    inout_writer_ptr->write_u64   (in_analysis.workload_reference.n_bytes);
    inout_writer_ptr->end_object();

    inout_writer_ptr->write_key("versions");
    inout_writer_ptr->begin_object();
    inout_writer_ptr->write_key("file_version");
    inout_writer_ptr->write_u64(PassesFormat::FILE_VERSION);
    inout_writer_ptr->write_key("key_scheme_version");
    inout_writer_ptr->write_u64(PassesFormat::KEY_SCHEME_VERSION);
    inout_writer_ptr->end_object();

    inout_writer_ptr->write_key   ("summary");
    inout_writer_ptr->begin_object();
    inout_writer_ptr->write_key   ("duration_seconds");
    inout_writer_ptr->write_double(get_seconds(in_analysis,
                                               in_analysis.summary.last_timestamp_qpc) );
    inout_writer_ptr->write_key   ("first_timestamp_qpc");
    inout_writer_ptr->write_u64   (in_analysis.summary.first_timestamp_qpc);
    inout_writer_ptr->write_key   ("is_truncated");
    inout_writer_ptr->write_bool  (in_analysis.summary.is_truncated);
    inout_writer_ptr->write_key   ("last_timestamp_qpc");
    inout_writer_ptr->write_u64   (in_analysis.summary.last_timestamp_qpc);
    inout_writer_ptr->write_key   ("n_calls");
    inout_writer_ptr->write_u64   (in_analysis.summary.n_calls);
    inout_writer_ptr->write_key   ("n_calls_by_api");
    inout_writer_ptr->begin_object();

    for (const auto& group_pair : in_analysis.summary.n_calls_by_group_map)
    {
        inout_writer_ptr->write_key(Capture::APIDumpLoader::get_api_group_name(group_pair.first) );
        inout_writer_ptr->write_u64(group_pair.second);
    }

    inout_writer_ptr->end_object();
    inout_writer_ptr->write_key   ("n_calls_ignored");
    inout_writer_ptr->write_u64   (in_analysis.summary.n_calls_ignored);
    inout_writer_ptr->write_key   ("n_resolution_hazards");
    inout_writer_ptr->begin_object();

    for (const auto& hazard_pair : in_analysis.summary.n_resolution_hazards_map)
    {
        inout_writer_ptr->write_key(get_resolution_hazard_name(hazard_pair.first) );
        inout_writer_ptr->write_u64(hazard_pair.second);
    }

    inout_writer_ptr->end_object();
    inout_writer_ptr->write_key   ("n_segments");
    inout_writer_ptr->write_u64   (in_analysis.summary.n_segments);
    inout_writer_ptr->write_key   ("n_trailing_calls");
    inout_writer_ptr->write_u64   (in_analysis.summary.n_trailing_calls);
    inout_writer_ptr->write_key   ("n_unfollowed_calls");
    inout_writer_ptr->write_u64   (in_analysis.summary.n_unfollowed_calls);
    inout_writer_ptr->write_key   ("n_unsupported_calls");
    inout_writer_ptr->write_u64   (in_analysis.summary.n_unsupported_calls);
    inout_writer_ptr->write_key   ("pointer_n_bytes");
    inout_writer_ptr->write_u64   (in_analysis.summary.pointer_n_bytes);
    inout_writer_ptr->write_key   ("qpc_frequency");
    inout_writer_ptr->write_u64   (in_analysis.summary.qpc_frequency);
    inout_writer_ptr->end_object();

    inout_writer_ptr->write_key("counts");
    inout_writer_ptr->begin_object();
    inout_writer_ptr->write_key("draw_structures");
    inout_writer_ptr->write_u64(model_ref.draw_structure_map.size() );
    inout_writer_ptr->write_key("draws");
    inout_writer_ptr->write_u64(n_draws);
    inout_writer_ptr->write_key("frame_shapes");
    inout_writer_ptr->write_u64(model_ref.frame_shape_interner.get_n_structures() );
    inout_writer_ptr->write_key("frames");
    inout_writer_ptr->write_u64(model_ref.frame_record_vec.size() );
    inout_writer_ptr->write_key("key_collisions");
    inout_writer_ptr->write_u64(model_ref.n_key_collisions);
    inout_writer_ptr->write_key("pass_structures");
    inout_writer_ptr->write_u64(model_ref.pass_structure_interner.get_n_structures() );
    inout_writer_ptr->write_key("resources");
    inout_writer_ptr->write_u64(model_ref.resource_vec.size() );
    inout_writer_ptr->write_key("resources_by_access");
    inout_writer_ptr->begin_object();

    for (const auto& count_pair : ResourceAccessTracker::count_by_class(model_ref.resource_access_vec) )
    {
        inout_writer_ptr->write_key(get_resource_access_class_name(count_pair.first) );
        inout_writer_ptr->write_u64(count_pair.second);
    }

    inout_writer_ptr->end_object();
    inout_writer_ptr->write_key("runs");
    inout_writer_ptr->write_u64(timeline.run_vec.size() );
    inout_writer_ptr->write_key("setup_frames");
    inout_writer_ptr->write_u64(timeline.setup_frame_index_vec.size() );
    inout_writer_ptr->end_object();

    inout_writer_ptr->end_object();

    return QueryStatus::OK;
}

Analysis::APIDumpAnalysis::QueryStatus Analysis::APIDumpAnalysis::QueryCommands::run_overlays(const AnalysisResult& in_analysis,
                                                                                              JsonWriter*           inout_writer_ptr)
{
    const auto&                 model_ref   = in_analysis.model;
    const std::vector<uint32_t> pass_id_vec = get_static_camera_pass_id_vec(in_analysis);
    const Timeline              timeline    = TimelineBuilder::build(in_analysis);

    inout_writer_ptr->begin_object();

    inout_writer_ptr->write_key   ("note");
    inout_writer_ptr->write_string("A hint, not a fact: a pass with a camera which never changes may also be a 3D scene seen from one place. One whose vertices are at about one depth is "
                                   "likely an overlay; one whose vertices are at many depths (depth.is_flat is false) is a 3D scene, and is left out of the unaffected list.");

    inout_writer_ptr->write_key("candidates");
    inout_writer_ptr->begin_array();

    for (const auto& pass_id : pass_id_vec)
    {
        const auto& structure    = model_ref.pass_structure_interner.get_structure(pass_id);
        const auto  depth_spread = get_pass_depth_spread(in_analysis,
                                                         pass_id);

        inout_writer_ptr->begin_object();
        inout_writer_ptr->write_key    ("pass_id");
        inout_writer_ptr->write_u64    (pass_id);
        inout_writer_ptr->write_key    ("key");
        inout_writer_ptr->write_hex_u64(structure.key);
        inout_writer_ptr->write_key    ("n_frames");
        inout_writer_ptr->write_u64    (timeline.pass_usage_vec.at(pass_id).n_frames);
        inout_writer_ptr->write_key    ("n_draw_structures");
        inout_writer_ptr->write_u64    (structure.draw_structure_key_vec.size() );
        write_depth_spread(depth_spread,
                           inout_writer_ptr);
        inout_writer_ptr->end_object();
    }

    inout_writer_ptr->end_array();

    /* A draw of a candidate pass is a candidate too: a person looks at them one by one, and a recipe can tell them apart by their signatures. */
    inout_writer_ptr->write_key("draws");
    inout_writer_ptr->begin_array();

    for (const auto& pass_id : pass_id_vec)
    {
        const auto& structure         = model_ref.pass_structure_interner.get_structure(pass_id);
        uint64_t    first_frame_index = 0;
        const bool  has_first_frame   = get_first_frame_index(in_analysis,
                                                              pass_id,
                                                             &first_frame_index);
        const auto  depth_spread      = get_pass_depth_spread(in_analysis,
                                                              pass_id);

        for (const auto& draw_signature : structure.draw_signature_vec)
        {
            const auto draw_iterator = model_ref.draw_structure_map.find(draw_signature.structure_key);

            inout_writer_ptr->begin_object();

            inout_writer_ptr->write_key    ("signature_key");
            inout_writer_ptr->write_hex_u64(draw_signature.signature_key);
            inout_writer_ptr->write_key    ("structure_key");
            inout_writer_ptr->write_hex_u64(draw_signature.structure_key);
            inout_writer_ptr->write_key    ("pass_id");
            inout_writer_ptr->write_u64    (pass_id);
            inout_writer_ptr->write_key    ("pass_key");
            inout_writer_ptr->write_hex_u64(structure.key);
            inout_writer_ptr->write_key    ("n_frames");
            inout_writer_ptr->write_u64    (timeline.pass_usage_vec.at(pass_id).n_frames);

            if (has_first_frame)
            {
                inout_writer_ptr->write_key("first_frame_index");
                inout_writer_ptr->write_u64(first_frame_index);
            }

            if (draw_iterator != model_ref.draw_structure_map.end() )
            {
                inout_writer_ptr->write_key    ("topology");
                inout_writer_ptr->write_string (get_primitive_topology_name(draw_iterator->second.primitive_topology) );
                inout_writer_ptr->write_key    ("is_indexed");
                inout_writer_ptr->write_bool   (draw_iterator->second.is_indexed);
                inout_writer_ptr->write_key    ("vertex_layout_key");
                inout_writer_ptr->write_hex_u64(draw_iterator->second.vertex_layout_key);
            }

            write_depth_spread(depth_spread,
                               inout_writer_ptr);

            /* What makes the draw a candidate; a hint as the pass is. */
            inout_writer_ptr->write_key("reasons");
            inout_writer_ptr->begin_inline_array();
            inout_writer_ptr->write_string("static_camera");

            if (depth_spread.has_depth)
            {
                inout_writer_ptr->write_string((depth_spread.is_flat) ? "flat_depth"
                                                                      : "deep_depth");
            }

            if (structure.clears_color == false)
            {
                inout_writer_ptr->write_string("draws_over_existing_color");
            }

            inout_writer_ptr->end_array();

            inout_writer_ptr->end_object();
        }
    }

    inout_writer_ptr->end_array();

    inout_writer_ptr->write_key("unaffected");
    inout_writer_ptr->begin_object();
    inout_writer_ptr->write_key("pass_keys");
    inout_writer_ptr->begin_array();

    /* A pass whose vertices are at many depths is a 3D scene seen from one place, and is not a candidate for the list. */
    for (const auto& pass_id : pass_id_vec)
    {
        const auto depth_spread = get_pass_depth_spread(in_analysis,
                                                        pass_id);

        if (depth_spread.has_depth == false ||
            depth_spread.is_flat)
        {
            inout_writer_ptr->write_hex_u64(model_ref.pass_structure_interner.get_structure(pass_id).key);
        }
    }

    inout_writer_ptr->end_array();
    inout_writer_ptr->end_object();

    inout_writer_ptr->end_object();

    return QueryStatus::OK;
}

Analysis::APIDumpAnalysis::QueryStatus Analysis::APIDumpAnalysis::QueryCommands::run_pass(const AnalysisResult& in_analysis,
                                                                                          const QueryArguments& in_arguments,
                                                                                          JsonWriter*           inout_writer_ptr,
                                                                                          std::string*          out_error_ptr)
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
                const uint64_t key         = structure.draw_structure_key_vec.at(static_cast<size_t>(n_draw) );
                const auto     draw_iter   = model_ref.draw_structure_map.find(key);

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

Analysis::APIDumpAnalysis::QueryStatus Analysis::APIDumpAnalysis::QueryCommands::run_pipelines(const AnalysisResult& in_analysis,
                                                                                               const QueryArguments& in_arguments,
                                                                                               JsonWriter*           inout_writer_ptr)
{
    const auto&    model_ref = in_analysis.model;
    const uint64_t n_shapes  = model_ref.frame_shape_interner.get_n_structures();
    const Timeline timeline  = TimelineBuilder::build(in_analysis);

    inout_writer_ptr->begin_object();

    write_page(n_shapes,
               in_arguments,
               inout_writer_ptr);

    inout_writer_ptr->write_key("shapes");
    inout_writer_ptr->begin_array();

    for (uint64_t n_shape = std::min    (in_arguments.offset,
                                         n_shapes);
                  n_shape < get_page_end(n_shapes,
                                         in_arguments);
                ++n_shape)
    {
        const auto& shape = model_ref.frame_shape_interner.get_structure(static_cast<uint32_t>(n_shape) );
        const auto& usage = timeline.shape_usage_vec.at                 (static_cast<size_t>  (n_shape) );

        inout_writer_ptr->begin_object();
        inout_writer_ptr->write_key   ("shape_id");
        inout_writer_ptr->write_u64   (n_shape);
        inout_writer_ptr->write_key    ("shape_key");
        inout_writer_ptr->write_hex_u64(ModelBuilder::compute_shape_key(model_ref,
                                                                        static_cast<uint32_t>(n_shape) ));
        inout_writer_ptr->write_key   ("n_frames");
        inout_writer_ptr->write_u64   (usage.n_frames);
        inout_writer_ptr->write_key   ("n_runs");
        inout_writer_ptr->write_u64   (usage.n_runs);
        inout_writer_ptr->write_key   ("first_seconds");
        inout_writer_ptr->write_double(usage.first_seconds);
        inout_writer_ptr->write_key   ("last_seconds");
        inout_writer_ptr->write_double(usage.last_seconds);
        inout_writer_ptr->write_key   ("passes");
        inout_writer_ptr->begin_array();

        /* IDs are sorted, so the instances of a pass structure are adjacent. */
        for (size_t n_id = 0;
                    n_id < shape.pass_structure_id_vec.size();
                  )
        {
            const uint32_t pass_id     = shape.pass_structure_id_vec.at(n_id);
            uint64_t       n_instances = 0;

            while (n_id                                 <  shape.pass_structure_id_vec.size() &&
                   shape.pass_structure_id_vec.at(n_id) == pass_id)
            {
                ++n_instances;
                ++n_id;
            }

            inout_writer_ptr->begin_object();
            inout_writer_ptr->write_key("pass_id");
            inout_writer_ptr->write_u64(pass_id);
            inout_writer_ptr->write_key("n_instances");
            inout_writer_ptr->write_u64(n_instances);
            inout_writer_ptr->end_object();
        }

        inout_writer_ptr->end_array();
        inout_writer_ptr->end_object();
    }

    inout_writer_ptr->end_array();
    inout_writer_ptr->end_object();

    return QueryStatus::OK;
}

Analysis::APIDumpAnalysis::QueryStatus Analysis::APIDumpAnalysis::QueryCommands::run_resources(const AnalysisResult& in_analysis,
                                                                                               const QueryArguments& in_arguments,
                                                                                               JsonWriter*           inout_writer_ptr)
{
    const auto          count_map = ResourceAccessTracker::count_by_class(in_analysis.model.resource_access_vec);
    std::vector<size_t> match_vec;
    const auto&         model_ref = in_analysis.model;

    for (size_t n_resource = 0;
                n_resource < model_ref.resource_access_vec.size();
              ++n_resource)
    {
        bool is_match = (in_arguments.has_access_class == false);

        if (is_match == false)
        {
            const auto access_class = ResourceAccessTracker::classify(model_ref.resource_access_vec.at(n_resource) );

            is_match = (access_class == in_arguments.access_class);
        }

        if (is_match)
        {
            match_vec.push_back(n_resource);
        }
    }

    inout_writer_ptr->begin_object();

    inout_writer_ptr->write_key("counts");
    inout_writer_ptr->begin_object();

    for (const auto& count_pair : count_map)
    {
        inout_writer_ptr->write_key(get_resource_access_class_name(count_pair.first) );
        inout_writer_ptr->write_u64(count_pair.second);
    }

    inout_writer_ptr->end_object();

    inout_writer_ptr->write_key("resources");
    inout_writer_ptr->begin_object();

    write_page(match_vec.size(),
               in_arguments,
               inout_writer_ptr);

    inout_writer_ptr->write_key("items");
    inout_writer_ptr->begin_array();

    for (uint64_t n_match = std::min    (in_arguments.offset,
                                         static_cast<uint64_t>(match_vec.size() ));
                  n_match < get_page_end(match_vec.size(),
                                         in_arguments);
                ++n_match)
    {
        const size_t n_resource = match_vec.at                    (static_cast<size_t>(n_match) );
        const auto&  access     = model_ref.resource_access_vec.at(n_resource);
        const auto&  resource   = model_ref.resource_vec.at       (n_resource);
        ResourceIdentity identity;

        ResourceIdentities::find(model_ref.resource_vec,
                                 make_resource_id(static_cast<uint32_t>(n_resource + 1) ),
                                &identity);

        inout_writer_ptr->begin_object();
        inout_writer_ptr->write_key   ("resource_id");
        inout_writer_ptr->write_u64   (n_resource + 1);
        inout_writer_ptr->write_key   ("name");
        inout_writer_ptr->write_string(resource.debug_name);

        inout_writer_ptr->write_key    ("description_key");
        inout_writer_ptr->write_hex_u64(identity.description_key);

        inout_writer_ptr->write_key("ordinal");
        inout_writer_ptr->write_u64(identity.ordinal);

        inout_writer_ptr->write_key   ("kind");
        inout_writer_ptr->write_string(get_resource_kind_name(resource.description.kind) );
        inout_writer_ptr->write_key   ("width");
        inout_writer_ptr->write_u64   (resource.description.width);
        inout_writer_ptr->write_key   ("height");
        inout_writer_ptr->write_u64   (resource.description.height);
        inout_writer_ptr->write_key   ("access");
        inout_writer_ptr->write_string(get_resource_access_class_name(ResourceAccessTracker::classify(access) ));
        inout_writer_ptr->write_key   ("n_frames_read");
        inout_writer_ptr->write_u64   (access.n_frames_read);
        inout_writer_ptr->write_key   ("n_frames_written");
        inout_writer_ptr->write_u64   (access.n_frames_written);
        inout_writer_ptr->write_key   ("n_frames_loop_carried");
        inout_writer_ptr->write_u64   (access.n_frames_loop_carried);

        inout_writer_ptr->write_key("first_read_frame_index");

        if (access.n_frames_read > 0)
        {
            inout_writer_ptr->write_u64(access.first_read_frame_index);
        }
        else
        {
            inout_writer_ptr->write_null();
        }

        inout_writer_ptr->write_key("first_write_frame_index");

        if (access.n_frames_written > 0)
        {
            inout_writer_ptr->write_u64(access.first_write_frame_index);
        }
        else
        {
            inout_writer_ptr->write_null();
        }

        inout_writer_ptr->write_key("last_write_frame_index");

        if (access.n_frames_written > 0)
        {
            inout_writer_ptr->write_u64(access.last_write_frame_index);
        }
        else
        {
            inout_writer_ptr->write_null();
        }

        inout_writer_ptr->write_key ("created_at_call_index");
        inout_writer_ptr->write_u64 (resource.created_at_call_index);
        inout_writer_ptr->write_key ("is_destroyed");
        inout_writer_ptr->write_bool(resource.is_destroyed);
        inout_writer_ptr->end_object();
    }

    inout_writer_ptr->end_array();
    inout_writer_ptr->end_object();

    inout_writer_ptr->end_object();

    return QueryStatus::OK;
}

Analysis::APIDumpAnalysis::QueryStatus Analysis::APIDumpAnalysis::QueryCommands::run_timeline(const AnalysisResult& in_analysis,
                                                                                              const QueryArguments& in_arguments,
                                                                                              JsonWriter*           inout_writer_ptr)
{
    const Timeline timeline = TimelineBuilder::build(in_analysis);

    inout_writer_ptr->begin_object();

    inout_writer_ptr->write_key("runs");
    inout_writer_ptr->begin_object();

    write_page(timeline.run_vec.size(),
               in_arguments,
               inout_writer_ptr);

    inout_writer_ptr->write_key("items");
    inout_writer_ptr->begin_array();

    for (uint64_t n_run = std::min    (in_arguments.offset,
                                       static_cast<uint64_t>(timeline.run_vec.size() ));
                  n_run < get_page_end(timeline.run_vec.size(),
                                       in_arguments);
                ++n_run)
    {
        const auto& run = timeline.run_vec.at(static_cast<size_t>(n_run) );

        inout_writer_ptr->begin_object();
        inout_writer_ptr->write_key   ("index");
        inout_writer_ptr->write_u64   (n_run);
        inout_writer_ptr->write_key   ("shape_id");
        inout_writer_ptr->write_u64   (run.shape_id);
        inout_writer_ptr->write_key   ("first_frame_index");
        inout_writer_ptr->write_u64   (run.first_frame_index);
        inout_writer_ptr->write_key   ("n_frames");
        inout_writer_ptr->write_u64   (run.n_frames);
        inout_writer_ptr->write_key   ("begin_seconds");
        inout_writer_ptr->write_double(run.begin_seconds);
        inout_writer_ptr->write_key   ("end_seconds");
        inout_writer_ptr->write_double(run.end_seconds);
        inout_writer_ptr->write_key   ("fps");
        inout_writer_ptr->write_double(run.fps);
        inout_writer_ptr->write_key   ("n_draws");
        inout_writer_ptr->write_u64   (run.n_draws);
        inout_writer_ptr->end_object();
    }

    inout_writer_ptr->end_array();
    inout_writer_ptr->end_object();

    inout_writer_ptr->write_key("setup_frames");
    inout_writer_ptr->begin_object();

    write_page(timeline.setup_frame_index_vec.size(),
               in_arguments,
               inout_writer_ptr);

    inout_writer_ptr->write_key("items");
    inout_writer_ptr->begin_inline_array();

    for (uint64_t n_frame = std::min    (in_arguments.offset,
                                         static_cast<uint64_t>(timeline.setup_frame_index_vec.size() ));
                  n_frame < get_page_end(timeline.setup_frame_index_vec.size(),
                                         in_arguments);
                ++n_frame)
    {
        inout_writer_ptr->write_u64(timeline.setup_frame_index_vec.at(static_cast<size_t>(n_frame) ));
    }

    inout_writer_ptr->end_array();
    inout_writer_ptr->end_object();

    inout_writer_ptr->end_object();

    return QueryStatus::OK;
}

void Analysis::APIDumpAnalysis::QueryCommands::write_depth_spread(const PassDepthSpread& in_spread,
                                                                  JsonWriter*            inout_writer_ptr)
{
    inout_writer_ptr->write_key("depth");
    inout_writer_ptr->begin_object();
    inout_writer_ptr->write_key("has_depth");
    inout_writer_ptr->write_bool(in_spread.has_depth);

    if (in_spread.has_depth)
    {
        inout_writer_ptr->write_key  ("n_vertices");
        inout_writer_ptr->write_u64  (in_spread.n_vertices);
        inout_writer_ptr->write_key  ("near");
        inout_writer_ptr->write_float(in_spread.depth_near);
        inout_writer_ptr->write_key  ("median");
        inout_writer_ptr->write_float(in_spread.depth_median);
        inout_writer_ptr->write_key  ("far");
        inout_writer_ptr->write_float(in_spread.depth_far);
        inout_writer_ptr->write_key  ("far_to_near_ratio");
        inout_writer_ptr->write_float(in_spread.far_to_near_ratio);
        inout_writer_ptr->write_key  ("is_flat");
        inout_writer_ptr->write_bool (in_spread.is_flat);
    }

    inout_writer_ptr->end_object();
}

void Analysis::APIDumpAnalysis::QueryCommands::write_draw_structure(const uint64_t&      in_key,
                                                                    const DrawStructure& in_structure,
                                                                    JsonWriter*          inout_writer_ptr)
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

void Analysis::APIDumpAnalysis::QueryCommands::write_frame(const AnalysisResult&             in_analysis,
                                                           const uint64_t&                   in_frame_index,
                                                           const RepresentativeStore::Entry* in_entry_ptr,
                                                           JsonWriter*                       inout_writer_ptr)
{
    const auto& record = in_analysis.model.frame_record_vec.at(static_cast<size_t>(in_frame_index) );

    inout_writer_ptr->begin_object();

    inout_writer_ptr->write_key   ("frame_index");
    inout_writer_ptr->write_u64   (record.frame_index);
    inout_writer_ptr->write_key   ("shape_id");
    inout_writer_ptr->write_u64   (record.shape_id);
    inout_writer_ptr->write_key    ("shape_key");
    inout_writer_ptr->write_hex_u64(ModelBuilder::compute_shape_key(in_analysis.model,
                                                                    record.shape_id) );
    inout_writer_ptr->write_key   ("begin_seconds");
    inout_writer_ptr->write_double(get_seconds(in_analysis,
                                               record.begin_timestamp_qpc) );
    inout_writer_ptr->write_key   ("end_seconds");
    inout_writer_ptr->write_double(get_seconds(in_analysis,
                                               record.end_timestamp_qpc) );
    inout_writer_ptr->write_key   ("n_draws");
    inout_writer_ptr->write_u64   (record.n_draws);
    inout_writer_ptr->write_key   ("n_passes");
    inout_writer_ptr->write_u64   (record.n_passes);

    inout_writer_ptr->write_key("calls");
    inout_writer_ptr->begin_object();
    inout_writer_ptr->write_key("first_call_index");
    inout_writer_ptr->write_u64(record.first_call_index);
    inout_writer_ptr->write_key("last_call_index");
    inout_writer_ptr->write_u64(record.last_call_index);
    inout_writer_ptr->write_key("n_calls");
    inout_writer_ptr->write_u64(record.last_call_index - record.first_call_index + 1);
    inout_writer_ptr->write_key("printer_command");
    inout_writer_ptr->write_string("APIDumpPrinter \"" + in_analysis.workload_reference.filename + "\" --frame " +
                                   std::to_string(record.frame_index) );
    inout_writer_ptr->end_object();

    inout_writer_ptr->write_key ("has_detail");
    inout_writer_ptr->write_bool(in_entry_ptr != nullptr);

    if (in_entry_ptr != nullptr)
    {
        inout_writer_ptr->write_key("passes");
        inout_writer_ptr->begin_array();

        for (const auto& instance : in_entry_ptr->detail.pass_instance_vec)
        {
            inout_writer_ptr->begin_object();
            inout_writer_ptr->write_key   ("pass_id");
            inout_writer_ptr->write_u64   (instance.structure_id);
            inout_writer_ptr->write_key   ("begin_seconds");
            inout_writer_ptr->write_double(get_seconds(in_analysis,
                                                       instance.begin_timestamp_qpc) );
            inout_writer_ptr->write_key   ("begin_call_index");
            inout_writer_ptr->write_u64   (instance.begin_call_index);
            inout_writer_ptr->write_key   ("end_call_index");
            inout_writer_ptr->write_u64   (instance.end_call_index);
            inout_writer_ptr->write_key   ("n_draws");
            inout_writer_ptr->write_u64   (instance.n_draws);
            inout_writer_ptr->write_key   ("n_vertices");
            inout_writer_ptr->write_u64   (instance.n_vertices);
            inout_writer_ptr->end_object();
        }

        inout_writer_ptr->end_array();
        inout_writer_ptr->write_key("transfers");
        inout_writer_ptr->begin_array();

        for (const auto& transfer : in_entry_ptr->detail.transfer_record_vec)
        {
            inout_writer_ptr->begin_object();
            inout_writer_ptr->write_key   ("kind");
            inout_writer_ptr->write_string(get_transfer_kind_name(transfer.kind) );
            inout_writer_ptr->write_key   ("call_index");
            inout_writer_ptr->write_u64   (transfer.call_index);
            inout_writer_ptr->write_key   ("source");

            write_resource_view(in_analysis,
                                transfer.source,
                                inout_writer_ptr);

            inout_writer_ptr->write_key   ("destination");

            write_resource_view(in_analysis,
                                transfer.destination,
                                inout_writer_ptr);

            inout_writer_ptr->write_key    ("data_n_bytes");
            inout_writer_ptr->write_u64    (transfer.data.n_bytes);
            inout_writer_ptr->write_key    ("data_content_hash");
            inout_writer_ptr->write_hex_u64(transfer.data.content_hash);
            inout_writer_ptr->end_object();
        }

        inout_writer_ptr->end_array();
    }

    inout_writer_ptr->end_object();
}

void Analysis::APIDumpAnalysis::QueryCommands::write_page(const uint64_t&       in_total,
                                                          const QueryArguments& in_arguments,
                                                          JsonWriter*           inout_writer_ptr)
{
    inout_writer_ptr->write_key("total");
    inout_writer_ptr->write_u64(in_total);
    inout_writer_ptr->write_key("offset");
    inout_writer_ptr->write_u64(in_arguments.offset);
    inout_writer_ptr->write_key("limit");
    inout_writer_ptr->write_u64(in_arguments.limit);
}

void Analysis::APIDumpAnalysis::QueryCommands::write_resource_view(const AnalysisResult& in_analysis,
                                                                   const ResourceView&   in_view,
                                                                   JsonWriter*           inout_writer_ptr)
{
    const uint32_t resource_id_value = get_resource_id_value(in_view.resource_id);

    if (resource_id_value == 0                                       ||
        resource_id_value >  in_analysis.model.resource_vec.size() )
    {
        inout_writer_ptr->write_null();
    }
    else
    {
        const auto& resource = in_analysis.model.resource_vec.at(resource_id_value - 1);

        inout_writer_ptr->begin_object();
        inout_writer_ptr->write_key   ("resource_id");
        inout_writer_ptr->write_u64   (resource_id_value);
        inout_writer_ptr->write_key   ("name");
        inout_writer_ptr->write_string(resource.debug_name);
        inout_writer_ptr->write_key   ("kind");
        inout_writer_ptr->write_string(get_resource_kind_name(resource.description.kind) );
        inout_writer_ptr->write_key   ("mip_level");
        inout_writer_ptr->write_u64   (in_view.mip_level);
        inout_writer_ptr->write_key   ("layer");
        inout_writer_ptr->write_u64   (in_view.layer);
        inout_writer_ptr->end_object();
    }
}

void Analysis::APIDumpAnalysis::QueryCommands::write_sample(const AnalysisResult&          in_analysis,
                                                            const ValueTimeSeries::Sample& in_sample,
                                                            JsonWriter*                    inout_writer_ptr)
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

void Analysis::APIDumpAnalysis::QueryCommands::write_series(const AnalysisResult&  in_analysis,
                                                            const ValueTimeSeries& in_series,
                                                            JsonWriter*            inout_writer_ptr)
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
