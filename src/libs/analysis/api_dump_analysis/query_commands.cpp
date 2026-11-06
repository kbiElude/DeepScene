/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_analysis/query_commands.h"
#include "analysis/api_dump_analysis/model_builder.h"
#include "analysis/api_dump_analysis/passes_format.h"
#include "analysis/api_dump_analysis/resource_access_tracker.h"
#include "analysis/api_dump_analysis/timeline_builder.h"
#include <algorithm>

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

Analysis::APIDumpAnalysis::QueryStatus Analysis::APIDumpAnalysis::QueryCommands::run(const std::string&       in_command,
                                                                                     const AnalysisResult&    in_analysis,
                                                                                     const QueryArguments&    in_arguments,
                                                                                     Utils::Json::JsonWriter* inout_writer_ptr,
                                                                                     std::string*             out_error_ptr)
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

Analysis::APIDumpAnalysis::QueryStatus Analysis::APIDumpAnalysis::QueryCommands::run_info(const AnalysisResult&    in_analysis,
                                                                                          Utils::Json::JsonWriter* inout_writer_ptr)
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

Analysis::APIDumpAnalysis::QueryStatus Analysis::APIDumpAnalysis::QueryCommands::run_pipelines(const AnalysisResult&    in_analysis,
                                                                                               const QueryArguments&    in_arguments,
                                                                                               Utils::Json::JsonWriter* inout_writer_ptr)
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

Analysis::APIDumpAnalysis::QueryStatus Analysis::APIDumpAnalysis::QueryCommands::run_timeline(const AnalysisResult&    in_analysis,
                                                                                              const QueryArguments&    in_arguments,
                                                                                              Utils::Json::JsonWriter* inout_writer_ptr)
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

void Analysis::APIDumpAnalysis::QueryCommands::write_page(const uint64_t&          in_total,
                                                          const QueryArguments&    in_arguments,
                                                          Utils::Json::JsonWriter* inout_writer_ptr)
{
    inout_writer_ptr->write_key("total");
    inout_writer_ptr->write_u64(in_total);
    inout_writer_ptr->write_key("offset");
    inout_writer_ptr->write_u64(in_arguments.offset);
    inout_writer_ptr->write_key("limit");
    inout_writer_ptr->write_u64(in_arguments.limit);
}
