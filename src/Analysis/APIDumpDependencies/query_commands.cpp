/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "Analysis/APIDumpAnalysis/query_commands.h"
#include "Analysis/APIDumpAnalysis/resource_identity.h"
#include "Analysis/APIDumpDependencies/deps_format.h"
#include "Analysis/APIDumpDependencies/frame_slice_planner.h"
#include "Analysis/APIDumpDependencies/per_eye_candidates.h"
#include "Analysis/APIDumpDependencies/query_commands.h"
#include <algorithm>
#include <map>

namespace
{
    using Analysis::APIDumpAnalysis::JsonWriter;
    using Analysis::APIDumpAnalysis::QueryStatus;
}

const Analysis::APIDumpDependencies::ResourceVersion* Analysis::APIDumpDependencies::QueryCommands::find_consumed_version_ptr(const ResourceDependencies& in_dependencies,
                                                                                                                              const uint64_t&             in_frame_index)
{
    /* The versions are in the order of their writers; the frame uses the last one which a frame before it wrote (or the one that nobody wrote). */
    const auto end_iterator = std::partition_point(in_dependencies.version_vec.begin(),
                                                   in_dependencies.version_vec.end(),
                                                   [&](const ResourceVersion& in_version)
                                                   {
                                                       return (in_version.has_writer == false ||
                                                               in_version.writer_frame_index < in_frame_index);
                                                   });

    return (end_iterator == in_dependencies.version_vec.begin() ) ? nullptr
                                                                  : &*(end_iterator - 1);
}

const Analysis::APIDumpDependencies::UseRun* Analysis::APIDumpDependencies::QueryCommands::find_use_run_ptr(const ResourceDependencies& in_dependencies,
                                                                                                            const uint64_t&             in_frame_index)
{
    /* The runs are in order and do not overlap: the one that the frame is in is the first which does not end before it. */
    const auto run_iterator = std::lower_bound(in_dependencies.use_run_vec.begin(),
                                               in_dependencies.use_run_vec.end(),
                                               in_frame_index,
                                               [](const UseRun& in_run,
                                                  const uint64_t& in_value)
                                               {
                                                   return (in_run.last_frame_index < in_value);
                                               });

    return (run_iterator != in_dependencies.use_run_vec.end()  &&
            run_iterator->first_frame_index <= in_frame_index) ? &*run_iterator
                                                               : nullptr;
}

const char* Analysis::APIDumpDependencies::QueryCommands::get_carry_kind_name(const CarryKind& in_kind)
{
    const char* result_ptr = "none";

    switch (in_kind)
    {
        case CarryKind::CERTAIN:
        {
            result_ptr = "certain";

            break;
        }

        case CarryKind::POSSIBLE:
        {
            result_ptr = "possible";

            break;
        }

        case CarryKind::NONE:
        default:
        {
            break;
        }
    }

    return result_ptr;
}

const std::vector<std::string>& Analysis::APIDumpDependencies::QueryCommands::get_command_names()
{
    static const std::vector<std::string> command_name_vec =
    {
        "carries",
        "frame",
        "info",
        "per-eye",
        "resource",
        "resources",
        "slice"
    };

    return command_name_vec;
}

const char* Analysis::APIDumpDependencies::QueryCommands::get_dependency_class_name(const DependencyClass& in_class)
{
    const char* result_ptr = "unused";

    switch (in_class)
    {
        case DependencyClass::CARRIED:
        {
            result_ptr = "carried";

            break;
        }

        case DependencyClass::CARRIED_POSSIBLY:
        {
            result_ptr = "carried_possibly";

            break;
        }

        case DependencyClass::FRAME_LOCAL:
        {
            result_ptr = "frame_local";

            break;
        }

        case DependencyClass::STATIC:
        {
            result_ptr = "static";

            break;
        }

        case DependencyClass::UNUSED:
        default:
        {
            break;
        }
    }

    return result_ptr;
}

Analysis::APIDumpAnalysis::QueryArguments Analysis::APIDumpDependencies::QueryCommands::get_paging(const QueryArguments& in_arguments)
{
    Analysis::APIDumpAnalysis::QueryArguments result;

    result.limit  = in_arguments.limit;
    result.offset = in_arguments.offset;

    return result;
}

bool Analysis::APIDumpDependencies::QueryCommands::parse_dependency_class(const std::string& in_name,
                                                                          DependencyClass*   out_class_ptr)
{
    static const std::map<std::string, DependencyClass> class_map =
    {
        {"carried",          DependencyClass::CARRIED},
        {"carried_possibly", DependencyClass::CARRIED_POSSIBLY},
        {"frame_local",      DependencyClass::FRAME_LOCAL},
        {"static",           DependencyClass::STATIC},
        {"unused",           DependencyClass::UNUSED}
    };

    const auto class_iterator = class_map.find(in_name);

    if (class_iterator != class_map.end() )
    {
        *out_class_ptr = class_iterator->second;
    }

    return (class_iterator != class_map.end() );
}

QueryStatus Analysis::APIDumpDependencies::QueryCommands::run(const std::string&        in_command,
                                                              const DependencyAnalysis& in_analysis,
                                                              const QueryArguments&     in_arguments,
                                                              JsonWriter*               inout_writer_ptr,
                                                              std::string*              out_error_ptr)
{
    QueryStatus result = QueryStatus::BAD_ARGUMENTS;

    if (in_command == "carries")
    {
        result = run_carries(in_analysis,
                             in_arguments,
                             inout_writer_ptr,
                             out_error_ptr);
    }
    else
    if (in_command == "frame")
    {
        result = run_frame(in_analysis,
                           in_arguments,
                           false, /* in_is_carries_only */
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
    if (in_command == "per-eye")
    {
        result = run_per_eye(in_analysis,
                             inout_writer_ptr);
    }
    else
    if (in_command == "resource")
    {
        result = run_resource(in_analysis,
                              in_arguments,
                              inout_writer_ptr,
                              out_error_ptr);
    }
    else
    if (in_command == "resources")
    {
        result = run_resources(in_analysis,
                               in_arguments,
                               inout_writer_ptr);
    }
    else
    if (in_command == "slice")
    {
        result = run_slice(in_analysis,
                           in_arguments,
                           inout_writer_ptr,
                           out_error_ptr);
    }
    else
    {
        *out_error_ptr = "Unknown command " + in_command;
    }

    return result;
}

QueryStatus Analysis::APIDumpDependencies::QueryCommands::run_carries(const DependencyAnalysis& in_analysis,
                                                                      const QueryArguments&     in_arguments,
                                                                      JsonWriter*               inout_writer_ptr,
                                                                      std::string*              out_error_ptr)
{
    return run_frame(in_analysis,
                     in_arguments,
                     true, /* in_is_carries_only */
                     inout_writer_ptr,
                     out_error_ptr);
}

QueryStatus Analysis::APIDumpDependencies::QueryCommands::run_frame(const DependencyAnalysis& in_analysis,
                                                                    const QueryArguments&     in_arguments,
                                                                    const bool&               in_is_carries_only,
                                                                    JsonWriter*               inout_writer_ptr,
                                                                    std::string*              out_error_ptr)
{
    std::vector<size_t> n_resource_vec;
    QueryStatus         result = QueryStatus::BAD_ARGUMENTS;

    if (in_arguments.has_frame_index == false)
    {
        *out_error_ptr = "A frame index is needed";

        goto end;
    }

    if (in_arguments.frame_index >= in_analysis.result.n_frames)
    {
        *out_error_ptr = "There is no frame " + std::to_string(in_arguments.frame_index) + "; there are " +
                         std::to_string(in_analysis.result.n_frames);
        result         = QueryStatus::NOT_FOUND;

        goto end;
    }

    for (size_t n_resource = 0;
                n_resource < in_analysis.result.resource_vec.size();
              ++n_resource)
    {
        const UseRun* run_ptr = find_use_run_ptr(in_analysis.result.resource_vec.at(n_resource),
                                                 in_arguments.frame_index);

        if (run_ptr != nullptr                                        &&
            (in_is_carries_only == false                              ||
             run_ptr->use.carry != CarryKind::NONE) )
        {
            n_resource_vec.push_back(n_resource);
        }
    }

    inout_writer_ptr->begin_object();
    inout_writer_ptr->write_key   ("frame_index");
    inout_writer_ptr->write_u64   (in_arguments.frame_index);

    if (in_arguments.frame_index < in_analysis.frame_call_range_vec.size() )
    {
        const FrameCallRange& call_range = in_analysis.frame_call_range_vec.at(static_cast<size_t>(in_arguments.frame_index) );

        inout_writer_ptr->write_key   ("calls");
        inout_writer_ptr->begin_object();
        inout_writer_ptr->write_key   ("first_call_index");
        inout_writer_ptr->write_u64   (call_range.first_call_index);
        inout_writer_ptr->write_key   ("last_call_index");
        inout_writer_ptr->write_u64   (call_range.last_call_index);
        inout_writer_ptr->write_key   ("printer_command");
        inout_writer_ptr->write_string("APIDumpPrinter \"" + in_analysis.workload_reference.filename + "\" --frame " +
                                       std::to_string(in_arguments.frame_index) );
        inout_writer_ptr->end_object();
    }

    inout_writer_ptr->write_key   ("resources");
    inout_writer_ptr->begin_object();

    Analysis::APIDumpAnalysis::QueryCommands::write_page(n_resource_vec.size(),
                                                         get_paging(in_arguments),
                                                         inout_writer_ptr);

    inout_writer_ptr->write_key   ("items");
    inout_writer_ptr->begin_array ();

    for (uint64_t n_item = in_arguments.offset;
                  n_item < Analysis::APIDumpAnalysis::QueryCommands::get_page_end(n_resource_vec.size(),
                                                                                  get_paging(in_arguments) );
                ++n_item)
    {
        const size_t                n_resource   = n_resource_vec.at(static_cast<size_t>(n_item) );
        const ResourceDependencies& dependencies = in_analysis.result.resource_vec.at(n_resource);
        const UseRun*               run_ptr      = find_use_run_ptr(dependencies,
                                                                    in_arguments.frame_index);
        const ResourceVersion*      version_ptr  = (run_ptr->use.carry != CarryKind::NONE) ? find_consumed_version_ptr(dependencies,
                                                                                                                       in_arguments.frame_index)
                                                                                           : nullptr;

        inout_writer_ptr->begin_object();

        write_resource_head(in_analysis,
                            n_resource,
                            inout_writer_ptr);

        inout_writer_ptr->write_key   ("carry");
        inout_writer_ptr->write_string(get_carry_kind_name(run_ptr->use.carry) );
        inout_writer_ptr->write_key   ("is_fully_written");
        inout_writer_ptr->write_bool  (run_ptr->use.is_fully_written);
        inout_writer_ptr->write_key   ("is_read");
        inout_writer_ptr->write_bool  (run_ptr->use.is_read);
        inout_writer_ptr->write_key   ("is_self_sampled");
        inout_writer_ptr->write_bool  (run_ptr->use.is_self_sampled);
        inout_writer_ptr->write_key   ("is_written");
        inout_writer_ptr->write_bool  (run_ptr->use.is_written);

        if (version_ptr != nullptr)
        {
            inout_writer_ptr->write_key("content_of");
            inout_writer_ptr->begin_object();

            if (version_ptr->has_writer)
            {
                inout_writer_ptr->write_key("writer_frame_index");
                inout_writer_ptr->write_u64(version_ptr->writer_frame_index);
                inout_writer_ptr->write_key("distance");
                inout_writer_ptr->write_u64(in_arguments.frame_index - version_ptr->writer_frame_index);
            }
            else
            {
                inout_writer_ptr->write_key   ("writer_frame_index");
                inout_writer_ptr->write_null  ();
                inout_writer_ptr->write_key   ("note");
                inout_writer_ptr->write_string("what the application made the resource with");
            }

            inout_writer_ptr->write_key ("keeps_earlier_content");
            inout_writer_ptr->write_bool(version_ptr->keeps_earlier_content);
            inout_writer_ptr->end_object();
        }

        inout_writer_ptr->end_object();
    }

    inout_writer_ptr->end_array ();
    inout_writer_ptr->end_object();
    inout_writer_ptr->end_object();

    result = QueryStatus::OK;

end:
    return result;
}

QueryStatus Analysis::APIDumpDependencies::QueryCommands::run_info(const DependencyAnalysis& in_analysis,
                                                                   JsonWriter*               inout_writer_ptr)
{
    std::map<DependencyClass, uint64_t> n_resources_by_class_map;

    for (const auto& dependencies : in_analysis.result.resource_vec)
    {
        n_resources_by_class_map[dependencies.summary.dependency_class]++;
    }

    inout_writer_ptr->begin_object();

    inout_writer_ptr->write_key   ("workload");
    inout_writer_ptr->begin_object();
    inout_writer_ptr->write_key   ("filename");
    inout_writer_ptr->write_string(in_analysis.workload_reference.filename);
    inout_writer_ptr->write_key   ("header_hash");
    inout_writer_ptr->write_hex_u64(in_analysis.workload_reference.header_hash);
    inout_writer_ptr->write_key   ("middle_hash");
    inout_writer_ptr->write_hex_u64(in_analysis.workload_reference.middle_hash);
    inout_writer_ptr->write_key   ("n_bytes");
    inout_writer_ptr->write_u64   (in_analysis.workload_reference.n_bytes);
    inout_writer_ptr->write_key   ("tail_hash");
    inout_writer_ptr->write_hex_u64(in_analysis.workload_reference.tail_hash);
    inout_writer_ptr->end_object();

    inout_writer_ptr->write_key   ("versions");
    inout_writer_ptr->begin_object();
    inout_writer_ptr->write_key   ("analysis_version");
    inout_writer_ptr->write_u64   (DepsFormat::ANALYSIS_VERSION);
    inout_writer_ptr->write_key   ("file_version");
    inout_writer_ptr->write_u64   (DepsFormat::FILE_VERSION);
    inout_writer_ptr->end_object();

    inout_writer_ptr->write_key   ("summary");
    inout_writer_ptr->begin_object();
    inout_writer_ptr->write_key   ("is_truncated");
    inout_writer_ptr->write_bool  (in_analysis.is_truncated);
    inout_writer_ptr->write_key   ("n_calls");
    inout_writer_ptr->write_u64   (in_analysis.n_calls);
    inout_writer_ptr->write_key   ("n_calls_by_api");
    inout_writer_ptr->begin_object();

    for (const auto& api : in_analysis.n_calls_by_api_map)
    {
        inout_writer_ptr->write_key(api.first);
        inout_writer_ptr->write_u64(api.second);
    }

    inout_writer_ptr->end_object();
    inout_writer_ptr->write_key   ("n_frames");
    inout_writer_ptr->write_u64   (in_analysis.result.n_frames);
    inout_writer_ptr->write_key   ("n_resources");
    inout_writer_ptr->write_u64   (in_analysis.resource_vec.size() );
    inout_writer_ptr->write_key   ("pointer_n_bytes");
    inout_writer_ptr->write_u64   (in_analysis.pointer_n_bytes);
    inout_writer_ptr->write_key   ("qpc_frequency");
    inout_writer_ptr->write_u64   (in_analysis.qpc_frequency);
    inout_writer_ptr->end_object();

    inout_writer_ptr->write_key   ("resources_by_class");
    inout_writer_ptr->begin_object();

    for (const auto& dependency_class : {DependencyClass::CARRIED,
                                         DependencyClass::CARRIED_POSSIBLY,
                                         DependencyClass::FRAME_LOCAL,
                                         DependencyClass::STATIC,
                                         DependencyClass::UNUSED})
    {
        inout_writer_ptr->write_key(get_dependency_class_name(dependency_class) );
        inout_writer_ptr->write_u64(n_resources_by_class_map[dependency_class]);
    }

    inout_writer_ptr->end_object();

    inout_writer_ptr->end_object();

    return QueryStatus::OK;
}

QueryStatus Analysis::APIDumpDependencies::QueryCommands::run_per_eye(const DependencyAnalysis& in_analysis,
                                                                      JsonWriter*               inout_writer_ptr)
{
    inout_writer_ptr->begin_object();
    inout_writer_ptr->write_key   ("note");
    inout_writer_ptr->write_string("Resources which are rendered to, and which some frame uses what an earlier frame left in (whether that frame "
                                   "writes them too or not): each eye is to have a copy of them (a recipe lists them as per_eye_textures). One "
                                   "that is used only possibly is one whose old content may show, because only part of it was written before "
                                   "it was read. A resource of a rotation group (resources of one description which are written by turns) is listed if "
                                   "another of the group is: all of them are one thing for the eyes.");
    inout_writer_ptr->write_key   ("candidates");
    inout_writer_ptr->begin_array ();

    for (const auto& candidate : PerEyeCandidates::find(in_analysis) )
    {
        inout_writer_ptr->begin_object();

        write_resource_head(in_analysis,
                            candidate.n_resource,
                            inout_writer_ptr);

        inout_writer_ptr->write_key   ("certainty");
        inout_writer_ptr->write_string((candidate.is_certain) ? "certain"
                                                              : "possible");

        if (candidate.is_added_by_group)
        {
            inout_writer_ptr->write_key   ("added_by_group");
            inout_writer_ptr->write_bool  (true);
        }

        if (candidate.rotation_group_index != NO_ROTATION_GROUP)
        {
            inout_writer_ptr->write_key("rotation_group");
            inout_writer_ptr->write_u64(candidate.rotation_group_index);
        }

        write_summary(in_analysis.result.resource_vec.at(candidate.n_resource).summary,
                      inout_writer_ptr);

        inout_writer_ptr->end_object();
    }

    inout_writer_ptr->end_array ();

    inout_writer_ptr->write_key  ("rotation_groups");
    inout_writer_ptr->begin_array();

    for (const auto& group : PerEyeCandidates::find_rotation_groups(in_analysis) )
    {
        inout_writer_ptr->begin_object();

        inout_writer_ptr->write_key   ("order");
        inout_writer_ptr->write_string((group.has_regular_order) ? "cycle"
                                                                : "irregular");
        inout_writer_ptr->write_key   ("resources");
        inout_writer_ptr->begin_array ();

        for (const size_t n_resource : group.n_resource_vec)
        {
            inout_writer_ptr->write_u64(n_resource + 1);
        }

        inout_writer_ptr->end_array ();
        inout_writer_ptr->end_object();
    }

    inout_writer_ptr->end_array ();
    inout_writer_ptr->end_object();

    return QueryStatus::OK;
}

QueryStatus Analysis::APIDumpDependencies::QueryCommands::run_resource(const DependencyAnalysis& in_analysis,
                                                                       const QueryArguments&     in_arguments,
                                                                       JsonWriter*               inout_writer_ptr,
                                                                       std::string*              out_error_ptr)
{
    QueryStatus result = QueryStatus::BAD_ARGUMENTS;

    if (in_arguments.has_resource_id == false)
    {
        *out_error_ptr = "A resource id is needed";

        goto end;
    }

    if (in_arguments.resource_id == 0                                   ||
        in_arguments.resource_id >  in_analysis.resource_vec.size() )
    {
        *out_error_ptr = "There is no resource " + std::to_string(in_arguments.resource_id) + "; there are " +
                         std::to_string(in_analysis.resource_vec.size() );
        result         = QueryStatus::NOT_FOUND;

        goto end;
    }

    {
        const size_t                n_resource   = static_cast<size_t>(in_arguments.resource_id - 1);
        const ResourceDependencies& dependencies = in_analysis.result.resource_vec.at(n_resource);

        inout_writer_ptr->begin_object();

        write_resource_head(in_analysis,
                            n_resource,
                            inout_writer_ptr);
        write_summary      (dependencies.summary,
                            inout_writer_ptr);

        inout_writer_ptr->write_key   ("runs");
        inout_writer_ptr->begin_object();

        Analysis::APIDumpAnalysis::QueryCommands::write_page(dependencies.use_run_vec.size(),
                                                             get_paging(in_arguments),
                                                             inout_writer_ptr);

        inout_writer_ptr->write_key   ("items");
        inout_writer_ptr->begin_array ();

        for (uint64_t n_run = in_arguments.offset;
                      n_run < Analysis::APIDumpAnalysis::QueryCommands::get_page_end(dependencies.use_run_vec.size(),
                                                                                     get_paging(in_arguments) );
                    ++n_run)
        {
            const UseRun& run = dependencies.use_run_vec.at(static_cast<size_t>(n_run) );

            inout_writer_ptr->begin_object();
            inout_writer_ptr->write_key   ("first_frame_index");
            inout_writer_ptr->write_u64   (run.first_frame_index);
            inout_writer_ptr->write_key   ("last_frame_index");
            inout_writer_ptr->write_u64   (run.last_frame_index);
            inout_writer_ptr->write_key   ("carry");
            inout_writer_ptr->write_string(get_carry_kind_name(run.use.carry) );
            inout_writer_ptr->write_key   ("is_fully_written");
            inout_writer_ptr->write_bool  (run.use.is_fully_written);
            inout_writer_ptr->write_key   ("is_read");
            inout_writer_ptr->write_bool  (run.use.is_read);
            inout_writer_ptr->write_key   ("is_self_sampled");
            inout_writer_ptr->write_bool  (run.use.is_self_sampled);
            inout_writer_ptr->write_key   ("is_written");
            inout_writer_ptr->write_bool  (run.use.is_written);
            inout_writer_ptr->end_object();
        }

        inout_writer_ptr->end_array ();
        inout_writer_ptr->end_object();

        inout_writer_ptr->write_key   ("versions");
        inout_writer_ptr->begin_object();

        Analysis::APIDumpAnalysis::QueryCommands::write_page(dependencies.version_vec.size(),
                                                             get_paging(in_arguments),
                                                             inout_writer_ptr);

        inout_writer_ptr->write_key   ("items");
        inout_writer_ptr->begin_array ();

        for (uint64_t n_version = in_arguments.offset;
                      n_version < Analysis::APIDumpAnalysis::QueryCommands::get_page_end(dependencies.version_vec.size(),
                                                                                         get_paging(in_arguments) );
                    ++n_version)
        {
            const ResourceVersion& version = dependencies.version_vec.at(static_cast<size_t>(n_version) );

            inout_writer_ptr->begin_object();
            inout_writer_ptr->write_key("writer_frame_index");

            if (version.has_writer)
            {
                inout_writer_ptr->write_u64(version.writer_frame_index);
            }
            else
            {
                inout_writer_ptr->write_null();
            }

            inout_writer_ptr->write_key ("keeps_earlier_content");
            inout_writer_ptr->write_bool(version.keeps_earlier_content);
            inout_writer_ptr->write_key ("n_consumer_frames");
            inout_writer_ptr->write_u64 (version.n_consumer_frames);
            inout_writer_ptr->write_key ("n_possible_consumer_frames");
            inout_writer_ptr->write_u64 (version.n_possible_consumer_frames);
            inout_writer_ptr->write_key ("consumers");
            inout_writer_ptr->begin_array();

            for (const auto& range : version.consumer_range_vec)
            {
                inout_writer_ptr->begin_inline_array();
                inout_writer_ptr->write_u64         (range.first_frame_index);
                inout_writer_ptr->write_u64         (range.last_frame_index);
                inout_writer_ptr->end_array         ();
            }

            inout_writer_ptr->end_array ();
            inout_writer_ptr->end_object();
        }

        inout_writer_ptr->end_array ();
        inout_writer_ptr->end_object();
        inout_writer_ptr->end_object();

        result = QueryStatus::OK;
    }

end:
    return result;
}

QueryStatus Analysis::APIDumpDependencies::QueryCommands::run_resources(const DependencyAnalysis& in_analysis,
                                                                        const QueryArguments&     in_arguments,
                                                                        JsonWriter*               inout_writer_ptr)
{
    std::vector<size_t> n_resource_vec;

    for (size_t n_resource = 0;
                n_resource < in_analysis.result.resource_vec.size();
              ++n_resource)
    {
        if (in_arguments.has_class == false                                                                      ||
            in_analysis.result.resource_vec.at(n_resource).summary.dependency_class == in_arguments.dependency_class)
        {
            n_resource_vec.push_back(n_resource);
        }
    }

    inout_writer_ptr->begin_object();
    inout_writer_ptr->write_key   ("resources");
    inout_writer_ptr->begin_object();

    Analysis::APIDumpAnalysis::QueryCommands::write_page(n_resource_vec.size(),
                                                         get_paging(in_arguments),
                                                         inout_writer_ptr);

    inout_writer_ptr->write_key   ("items");
    inout_writer_ptr->begin_array ();

    for (uint64_t n_item = in_arguments.offset;
                  n_item < Analysis::APIDumpAnalysis::QueryCommands::get_page_end(n_resource_vec.size(),
                                                                                  get_paging(in_arguments) );
                ++n_item)
    {
        const size_t n_resource = n_resource_vec.at(static_cast<size_t>(n_item) );

        inout_writer_ptr->begin_object();

        write_resource_head(in_analysis,
                            n_resource,
                            inout_writer_ptr);
        write_summary      (in_analysis.result.resource_vec.at(n_resource).summary,
                            inout_writer_ptr);

        inout_writer_ptr->end_object();
    }

    inout_writer_ptr->end_array ();
    inout_writer_ptr->end_object();
    inout_writer_ptr->end_object();

    return QueryStatus::OK;
}

QueryStatus Analysis::APIDumpDependencies::QueryCommands::run_slice(const DependencyAnalysis& in_analysis,
                                                                    const QueryArguments&     in_arguments,
                                                                    JsonWriter*               inout_writer_ptr,
                                                                    std::string*              out_error_ptr)
{
    FrameSlice  slice;
    QueryStatus result = QueryStatus::BAD_ARGUMENTS;

    if (in_arguments.has_frame_index == false)
    {
        *out_error_ptr = "A frame index is needed";

        goto end;
    }

    if (in_arguments.frame_index >= in_analysis.result.n_frames)
    {
        *out_error_ptr = "There is no frame " + std::to_string(in_arguments.frame_index) + "; there are " +
                         std::to_string(in_analysis.result.n_frames);
        result         = QueryStatus::NOT_FOUND;

        goto end;
    }

    slice = FrameSlicePlanner::plan(in_analysis,
                                    in_arguments.frame_index);

    inout_writer_ptr->begin_object();

    inout_writer_ptr->write_key("target_frame_index");
    inout_writer_ptr->write_u64(slice.target_frame_index);
    inout_writer_ptr->write_key("n_frames");
    inout_writer_ptr->write_u64(in_analysis.result.n_frames);

    /* The frames before the target which a replay has to make, and those it can leave out. */
    {
        uint64_t n_live_before   = 0;
        uint64_t n_dead          = 0;
        uint64_t n_frames_before = std::min<uint64_t>(slice.target_frame_index,
                                                      in_analysis.result.n_frames);

        for (uint64_t n_frame = 0;
                      n_frame < n_frames_before;
                    ++n_frame)
        {
            n_live_before += (FrameSlicePlanner::is_live(slice,
                                                         n_frame) ) ? 1
                                                                    : 0;
        }

        n_dead = n_frames_before - n_live_before;

        inout_writer_ptr->write_key("n_frames_before_target");
        inout_writer_ptr->write_u64(n_frames_before);
        inout_writer_ptr->write_key("n_live_before_target");
        inout_writer_ptr->write_u64(n_live_before);
        inout_writer_ptr->write_key("n_dead_before_target");
        inout_writer_ptr->write_u64(n_dead);
    }

    /* Runs of dead frames, the longest ones first would be a sort; in the order of the frames, paged, is what a person can look through. */
    {
        std::vector<FrameRange> dead_run_vec;

        for (uint64_t n_frame = 0;
                      n_frame < slice.target_frame_index;
                    ++n_frame)
        {
            if (FrameSlicePlanner::is_live(slice,
                                           n_frame) == false)
            {
                if (dead_run_vec.empty()                                         == false &&
                    dead_run_vec.back().last_frame_index + 1                     == n_frame)
                {
                    dead_run_vec.back().last_frame_index = n_frame;
                }
                else
                {
                    FrameRange range;

                    range.first_frame_index = n_frame;
                    range.last_frame_index  = n_frame;

                    dead_run_vec.push_back(range);
                }
            }
        }

        inout_writer_ptr->write_key("n_dead_runs");
        inout_writer_ptr->write_u64(dead_run_vec.size() );
        inout_writer_ptr->write_key("dead_runs");
        inout_writer_ptr->begin_array();

        for (uint64_t n_run = std::min<uint64_t>(in_arguments.offset,
                                                 dead_run_vec.size() );
                      n_run < dead_run_vec.size() &&
                      (in_arguments.limit == 0 || n_run < in_arguments.offset + in_arguments.limit);
                    ++n_run)
        {
            inout_writer_ptr->begin_inline_array();
            inout_writer_ptr->write_u64(dead_run_vec.at(static_cast<size_t>(n_run) ).first_frame_index);
            inout_writer_ptr->write_u64(dead_run_vec.at(static_cast<size_t>(n_run) ).last_frame_index);
            inout_writer_ptr->end_array();
        }

        inout_writer_ptr->end_array();
    }

    inout_writer_ptr->end_object();

    result = QueryStatus::OK;

end:
    return result;
}

void Analysis::APIDumpDependencies::QueryCommands::write_resource_head(const DependencyAnalysis& in_analysis,
                                                                       const size_t&             in_n_resource,
                                                                       JsonWriter*               inout_writer_ptr)
{
    const auto&                                   resource = in_analysis.resource_vec.at(in_n_resource);
    Analysis::APIDumpAnalysis::ResourceIdentity   identity;

    Analysis::APIDumpAnalysis::ResourceIdentities::find(in_analysis.resource_vec,
                                                        resource.id,
                                                       &identity);

    inout_writer_ptr->write_key    ("resource_id");
    inout_writer_ptr->write_u64    (in_n_resource + 1);
    inout_writer_ptr->write_key    ("name");
    inout_writer_ptr->write_string (resource.debug_name);
    inout_writer_ptr->write_key    ("description_key");
    inout_writer_ptr->write_hex_u64(identity.description_key);
    inout_writer_ptr->write_key    ("ordinal");
    inout_writer_ptr->write_u64    (identity.ordinal);
    inout_writer_ptr->write_key    ("kind");
    inout_writer_ptr->write_string (Analysis::APIDumpAnalysis::QueryCommands::get_resource_kind_name(resource.description.kind) );
    inout_writer_ptr->write_key    ("width");
    inout_writer_ptr->write_u64    (resource.description.width);
    inout_writer_ptr->write_key    ("height");
    inout_writer_ptr->write_u64    (resource.description.height);
    inout_writer_ptr->write_key    ("is_render_target");
    inout_writer_ptr->write_bool   (resource.description.is_render_target);
    inout_writer_ptr->write_key    ("is_depth_stencil");
    inout_writer_ptr->write_bool   (resource.description.is_depth_stencil);
    inout_writer_ptr->write_key    ("created_at_call_index");
    inout_writer_ptr->write_u64    (resource.created_at_call_index);
}

void Analysis::APIDumpDependencies::QueryCommands::write_summary(const ResourceSummary& in_summary,
                                                                 JsonWriter*            inout_writer_ptr)
{
    inout_writer_ptr->write_key   ("class");
    inout_writer_ptr->write_string(get_dependency_class_name(in_summary.dependency_class) );
    inout_writer_ptr->write_key   ("summary");
    inout_writer_ptr->begin_object();
    inout_writer_ptr->write_key   ("first_frame_index");
    inout_writer_ptr->write_u64   (in_summary.first_frame_index);
    inout_writer_ptr->write_key   ("last_frame_index");
    inout_writer_ptr->write_u64   (in_summary.last_frame_index);
    inout_writer_ptr->write_key   ("longest_carry_n_frames");
    inout_writer_ptr->write_u64   (in_summary.longest_carry_n_frames);
    inout_writer_ptr->write_key   ("n_frames_carrying");
    inout_writer_ptr->write_u64   (in_summary.n_frames_carrying);
    inout_writer_ptr->write_key   ("n_frames_carrying_possibly");
    inout_writer_ptr->write_u64   (in_summary.n_frames_carrying_possibly);
    inout_writer_ptr->write_key   ("n_frames_read");
    inout_writer_ptr->write_u64   (in_summary.n_frames_read);
    inout_writer_ptr->write_key   ("n_frames_self_sampled");
    inout_writer_ptr->write_u64   (in_summary.n_frames_self_sampled);
    inout_writer_ptr->write_key   ("n_frames_used");
    inout_writer_ptr->write_u64   (in_summary.n_frames_used);
    inout_writer_ptr->write_key   ("n_frames_using_earlier_content");
    inout_writer_ptr->write_u64   (in_summary.n_frames_using_earlier_content);
    inout_writer_ptr->write_key   ("n_frames_using_earlier_content_possibly");
    inout_writer_ptr->write_u64   (in_summary.n_frames_using_earlier_content_possibly);
    inout_writer_ptr->write_key   ("n_frames_written");
    inout_writer_ptr->write_u64   (in_summary.n_frames_written);
    inout_writer_ptr->end_object();
}
