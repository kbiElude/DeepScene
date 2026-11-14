/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_analysis/query_commands.h"
#include "analysis/api_dump_textures/query_commands.h"
#include "analysis/api_dump_textures/texture_level_content.h"
#include "common/macros.h"
#include <array>
#include <map>
#include <set>

const Analysis::APIDumpAnalysis::Resource* Analysis::APIDumpTextures::QueryCommands::find_resource_ptr(const TextureTimeline& in_timeline,
                                                                                                       const uint64_t&        in_resource_id)
{
    const Analysis::APIDumpAnalysis::Resource* result_ptr = nullptr;

    for (const auto& resource : in_timeline.resource_vec)
    {
        if (Analysis::APIDumpAnalysis::get_resource_id_value(resource.id) == in_resource_id)
        {
            result_ptr = &resource;

            break;
        }
    }

    return result_ptr;
}

const std::vector<std::string>& Analysis::APIDumpTextures::QueryCommands::get_command_names()
{
    static const std::vector<std::string> command_name_vec =
    {
        "at",
        "info",
        "resource",
        "resources"
    };

    return command_name_vec;
}

const char* Analysis::APIDumpTextures::QueryCommands::get_event_kind_name(const TextureEventKind& in_kind)
{
    const char* result_ptr = nullptr;

    switch (in_kind)
    {
        case TextureEventKind::GENERATE_MIPS:
        {
            result_ptr = "generate_mips";

            break;
        }

        case TextureEventKind::RENDER_WRITE:
        {
            result_ptr = "render_write";

            break;
        }

        case TextureEventKind::SNAPSHOT:
        {
            result_ptr = "snapshot";

            break;
        }

        case TextureEventKind::UPLOAD:
        {
            result_ptr = "upload";

            break;
        }

        default:
        {
            AI_FAIL_UNSUPPORTED(in_kind);
        }
    }

    return result_ptr;
}

Analysis::APIDumpAnalysis::QueryArguments Analysis::APIDumpTextures::QueryCommands::get_paging(const QueryArguments& in_arguments)
{
    Analysis::APIDumpAnalysis::QueryArguments result;

    result.limit  = in_arguments.limit;
    result.offset = in_arguments.offset;

    return result;
}

std::map<uint64_t, Analysis::APIDumpTextures::QueryCommands::ResourceUse> Analysis::APIDumpTextures::QueryCommands::get_resource_use_map(const TextureTimeline& in_timeline)
{
    std::map<uint64_t, ResourceUse> result;

    for (const auto& event : in_timeline.event_vec)
    {
        ResourceUse& use = result[Analysis::APIDumpAnalysis::get_resource_id_value(event.destination.resource_id)];

        use.first_frame = (event.frame_index < use.first_frame) ? event.frame_index
                                                                : use.first_frame;
        use.last_frame  = (event.frame_index > use.last_frame)  ? event.frame_index
                                                                : use.last_frame;

        use.level_set.insert(event.destination.mip_level);

        use.n_event_array.at(static_cast<size_t>(event.kind) )++;
    }

    return result;
}

Analysis::APIDumpAnalysis::QueryStatus Analysis::APIDumpTextures::QueryCommands::run(const std::string&       in_command,
                                                                                     const TextureTimeline&   in_timeline,
                                                                                     const QueryArguments&    in_arguments,
                                                                                     Utils::Json::JsonWriter* inout_writer_ptr,
                                                                                     std::string*             out_error_ptr)
{
    QueryStatus result = QueryStatus::BAD_ARGUMENTS;

    if (in_command == "at")
    {
        result = run_at(in_timeline,
                        in_arguments,
                        inout_writer_ptr,
                        out_error_ptr);
    }
    else
    if (in_command == "info")
    {
        result = run_info(in_timeline,
                          inout_writer_ptr);
    }
    else
    if (in_command == "resource")
    {
        result = run_resource(in_timeline,
                              in_arguments,
                              inout_writer_ptr,
                              out_error_ptr);
    }
    else
    if (in_command == "resources")
    {
        result = run_resources(in_timeline,
                               in_arguments,
                               inout_writer_ptr);
    }
    else
    {
        *out_error_ptr = "Unknown command " + in_command;
    }

    return result;
}

Analysis::APIDumpAnalysis::QueryStatus Analysis::APIDumpTextures::QueryCommands::run_at(const TextureTimeline&   in_timeline,
                                                                                        const QueryArguments&    in_arguments,
                                                                                        Utils::Json::JsonWriter* inout_writer_ptr,
                                                                                        std::string*             out_error_ptr)
{
    LevelContent                               content;
    LevelCut                                   cut;
    const Analysis::APIDumpAnalysis::Resource* resource_ptr = nullptr;
    QueryStatus                                result       = QueryStatus::BAD_ARGUMENTS;

    if (in_arguments.has_frame_index == false ||
        in_arguments.has_resource_id == false ||
        in_arguments.has_mip_level   == false)
    {
        *out_error_ptr = "A frame, a resource and a level are needed: at <frame> <resource> <level>";

        goto end;
    }

    resource_ptr = find_resource_ptr(in_timeline,
                                     in_arguments.resource_id);

    if (resource_ptr == nullptr)
    {
        *out_error_ptr = "There is no resource " + std::to_string(in_arguments.resource_id);
        result         = QueryStatus::NOT_FOUND;

        goto end;
    }

    cut.last_call_index  = (in_arguments.has_call_limit) ? in_arguments.call_limit
                                                         : NO_LIMIT;
    cut.last_frame_index = in_arguments.frame_index;

    content = TextureLevelContent::find(in_timeline,
                                        resource_ptr->id,
                                        static_cast<uint32_t>(in_arguments.mip_level),
                                        cut);

    inout_writer_ptr->begin_object();
    inout_writer_ptr->write_key   ("resource");
    inout_writer_ptr->begin_object();

    write_resource_head(*resource_ptr,
                        in_arguments.resource_id,
                        inout_writer_ptr);

    inout_writer_ptr->end_object  ();
    inout_writer_ptr->write_key   ("level");
    inout_writer_ptr->write_u64   (in_arguments.mip_level);
    inout_writer_ptr->write_key   ("last_frame_index");
    inout_writer_ptr->write_u64   (in_arguments.frame_index);

    if (in_arguments.has_call_limit)
    {
        inout_writer_ptr->write_key("last_call_index");
        inout_writer_ptr->write_u64(in_arguments.call_limit);
    }

    inout_writer_ptr->write_key   ("has_level");
    inout_writer_ptr->write_bool  (content.has_level);

    if (content.has_level)
    {
        uint64_t n_covered_texels = 0;

        inout_writer_ptr->write_key   ("size");
        inout_writer_ptr->begin_object();

        write_level_size(content.level_width,
                         content.level_height,
                         content.level_depth_or_layer_count,
                         content.level_format,
                         inout_writer_ptr);

        inout_writer_ptr->end_object  ();

        for (const auto& piece : content.piece_vec)
        {
            n_covered_texels += static_cast<uint64_t>(piece.width) * piece.height * piece.layer_count;
        }

        inout_writer_ptr->write_key   ("covered_texels");
        inout_writer_ptr->write_u64   (n_covered_texels);
        inout_writer_ptr->write_key   ("level_texels");
        inout_writer_ptr->write_u64   (static_cast<uint64_t>(content.level_width) * content.level_height * content.level_depth_or_layer_count);
    }

    inout_writer_ptr->write_key   ("pieces");
    inout_writer_ptr->begin_array ();

    for (const auto& piece : content.piece_vec)
    {
        /* The part of the level that the piece is (in texels), and the event that made it: the event has a region of its own, of which the piece is what no later
         * event wrote over.
         */
        inout_writer_ptr->begin_object();
        inout_writer_ptr->write_key   ("first_layer");
        inout_writer_ptr->write_u64   (piece.first_layer);
        inout_writer_ptr->write_key   ("layer_count");
        inout_writer_ptr->write_u64   (piece.layer_count);
        inout_writer_ptr->write_key   ("x");
        inout_writer_ptr->write_u64   (piece.x);
        inout_writer_ptr->write_key   ("y");
        inout_writer_ptr->write_u64   (piece.y);
        inout_writer_ptr->write_key   ("width");
        inout_writer_ptr->write_u64   (piece.width);
        inout_writer_ptr->write_key   ("height");
        inout_writer_ptr->write_u64   (piece.height);
        inout_writer_ptr->write_key   ("event");
        inout_writer_ptr->begin_object();

        write_event(in_timeline,
                    piece.event_index,
                    inout_writer_ptr);

        inout_writer_ptr->end_object  ();
        inout_writer_ptr->end_object  ();
    }

    inout_writer_ptr->end_array   ();
    inout_writer_ptr->write_key   ("snapshot");

    if (content.has_snapshot)
    {
        inout_writer_ptr->begin_object();

        write_event(in_timeline,
                    content.snapshot_event_index,
                    inout_writer_ptr);

        inout_writer_ptr->end_object  ();
    }
    else
    {
        inout_writer_ptr->write_null();
    }

    inout_writer_ptr->end_object  ();

    result = QueryStatus::OK;

end:
    return result;
}

Analysis::APIDumpAnalysis::QueryStatus Analysis::APIDumpTextures::QueryCommands::run_info(const TextureTimeline&   in_timeline,
                                                                                          Utils::Json::JsonWriter* inout_writer_ptr)
{
    std::array<uint64_t, N_EVENT_KINDS> n_event_array     = {};
    uint64_t                            n_stored_raw      = 0;
    uint64_t                            n_stored_contents = 0;
    uint64_t                            n_workload_raw    = 0;
    uint64_t                            n_workload_chunks = 0;

    for (const auto& event : in_timeline.event_vec)
    {
        n_event_array.at(static_cast<size_t>(event.kind) )++;
    }

    for (const auto& blob : in_timeline.blob_vec)
    {
        if (blob.location == BlobLocation::WORKLOAD_CHUNK)
        {
            ++n_workload_chunks;

            n_workload_raw += blob.n_raw_bytes;
        }
        else
        {
            ++n_stored_contents;

            n_stored_raw += blob.n_raw_bytes;
        }
    }

    inout_writer_ptr->begin_object();
    inout_writer_ptr->write_key   ("workload");
    inout_writer_ptr->write_string(in_timeline.workload_reference.filename);
    inout_writer_ptr->write_key   ("workload_n_bytes");
    inout_writer_ptr->write_u64   (in_timeline.workload_reference.n_bytes);
    inout_writer_ptr->write_key   ("includes_render_content");
    inout_writer_ptr->write_bool  (in_timeline.includes_render_content);
    inout_writer_ptr->write_key   ("n_events");
    inout_writer_ptr->write_u64   (in_timeline.event_vec.size() );

    write_event_counts(n_event_array,
                       inout_writer_ptr);

    inout_writer_ptr->write_key   ("n_resources");
    inout_writer_ptr->write_u64   (in_timeline.resource_vec.size() );
    inout_writer_ptr->write_key   ("n_resources_with_events");
    inout_writer_ptr->write_u64   (get_resource_use_map(in_timeline).size() );
    inout_writer_ptr->write_key   ("contents");
    inout_writer_ptr->begin_object();
    inout_writer_ptr->write_key   ("n_left_in_workload");
    inout_writer_ptr->write_u64   (n_workload_chunks);
    inout_writer_ptr->write_key   ("n_raw_bytes_left_in_workload");
    inout_writer_ptr->write_u64   (n_workload_raw);
    inout_writer_ptr->write_key   ("n_stored");
    inout_writer_ptr->write_u64   (n_stored_contents);
    inout_writer_ptr->write_key   ("n_raw_bytes_stored");
    inout_writer_ptr->write_u64   (n_stored_raw);
    inout_writer_ptr->write_key   ("blob_file_n_bytes");
    inout_writer_ptr->write_u64   (in_timeline.blob_file_n_bytes);
    inout_writer_ptr->end_object  ();
    inout_writer_ptr->end_object  ();

    return QueryStatus::OK;
}

Analysis::APIDumpAnalysis::QueryStatus Analysis::APIDumpTextures::QueryCommands::run_resource(const TextureTimeline&   in_timeline,
                                                                                              const QueryArguments&    in_arguments,
                                                                                              Utils::Json::JsonWriter* inout_writer_ptr,
                                                                                              std::string*             out_error_ptr)
{
    typedef Analysis::APIDumpAnalysis::QueryArguments PagingArguments;

    struct LevelUse
    {
        std::array<uint64_t, N_EVENT_KINDS> n_event_array = {};
        uint32_t                            last_event    = 0; /* the last one that is not a snapshot: it says the size and format the level has at the end */
        bool                                has_size      = false;
    };

    std::vector<uint32_t>                      event_index_vec;
    std::map<uint32_t, LevelUse>               level_map;
    const PagingArguments                      paging       = get_paging(in_arguments);
    const Analysis::APIDumpAnalysis::Resource* resource_ptr = nullptr;
    QueryStatus                                result       = QueryStatus::BAD_ARGUMENTS;

    if (in_arguments.has_resource_id == false)
    {
        *out_error_ptr = "A resource id is needed";

        goto end;
    }

    resource_ptr = find_resource_ptr(in_timeline,
                                     in_arguments.resource_id);

    if (resource_ptr == nullptr)
    {
        *out_error_ptr = "There is no resource " + std::to_string(in_arguments.resource_id);
        result         = QueryStatus::NOT_FOUND;

        goto end;
    }

    for (uint32_t n_event = 0;
                  n_event < in_timeline.event_vec.size();
                ++n_event)
    {
        const TextureEvent& event = in_timeline.event_vec.at(n_event);

        if (Analysis::APIDumpAnalysis::get_resource_id_value(event.destination.resource_id) == in_arguments.resource_id)
        {
            LevelUse& level_use = level_map[event.destination.mip_level];

            event_index_vec.push_back(n_event);

            level_use.n_event_array.at(static_cast<size_t>(event.kind) )++;

            if (event.kind != TextureEventKind::SNAPSHOT)
            {
                level_use.has_size   = true;
                level_use.last_event = n_event;
            }
        }
    }

    inout_writer_ptr->begin_object();
    inout_writer_ptr->write_key   ("resource");
    inout_writer_ptr->begin_object();

    write_resource_head(*resource_ptr,
                        in_arguments.resource_id,
                        inout_writer_ptr);

    inout_writer_ptr->end_object  ();
    inout_writer_ptr->write_key   ("levels");
    inout_writer_ptr->begin_array ();

    for (const auto& level_pair : level_map)
    {
        inout_writer_ptr->begin_object();
        inout_writer_ptr->write_key   ("level");
        inout_writer_ptr->write_u64   (level_pair.first);

        write_event_counts(level_pair.second.n_event_array,
                           inout_writer_ptr);

        if (level_pair.second.has_size)
        {
            const TextureEvent& last_event = in_timeline.event_vec.at(level_pair.second.last_event);

            write_level_size(last_event.level_width,
                             last_event.level_height,
                             last_event.level_depth_or_layer_count,
                             last_event.level_format,
                             inout_writer_ptr);
        }

        inout_writer_ptr->end_object  ();
    }

    inout_writer_ptr->end_array   ();
    inout_writer_ptr->write_key   ("events");
    inout_writer_ptr->begin_object();

    Analysis::APIDumpAnalysis::QueryCommands::write_page(event_index_vec.size(),
                                                         paging,
                                                         inout_writer_ptr);

    inout_writer_ptr->write_key   ("items");
    inout_writer_ptr->begin_array ();

    for (uint64_t n_item = in_arguments.offset;
                  n_item < Analysis::APIDumpAnalysis::QueryCommands::get_page_end(event_index_vec.size(),
                                                                                  paging);
                ++n_item)
    {
        inout_writer_ptr->begin_object();

        write_event(in_timeline,
                    event_index_vec.at(static_cast<size_t>(n_item) ),
                    inout_writer_ptr);

        inout_writer_ptr->end_object  ();
    }

    inout_writer_ptr->end_array   ();
    inout_writer_ptr->end_object  ();
    inout_writer_ptr->end_object  ();

    result = QueryStatus::OK;

end:
    return result;
}

Analysis::APIDumpAnalysis::QueryStatus Analysis::APIDumpTextures::QueryCommands::run_resources(const TextureTimeline&   in_timeline,
                                                                                               const QueryArguments&    in_arguments,
                                                                                               Utils::Json::JsonWriter* inout_writer_ptr)
{
    typedef Analysis::APIDumpAnalysis::QueryArguments PagingArguments;

    const auto            resource_use_map = get_resource_use_map(in_timeline);
    const PagingArguments paging           = get_paging(in_arguments);

    std::vector<uint64_t> resource_id_vec;

    for (const auto& use_pair : resource_use_map)
    {
        resource_id_vec.push_back(use_pair.first);
    }

    inout_writer_ptr->begin_object();
    inout_writer_ptr->write_key   ("resources");
    inout_writer_ptr->begin_object();

    Analysis::APIDumpAnalysis::QueryCommands::write_page(resource_id_vec.size(),
                                                         paging,
                                                         inout_writer_ptr);

    inout_writer_ptr->write_key   ("items");
    inout_writer_ptr->begin_array ();

    for (uint64_t n_item = in_arguments.offset;
                  n_item < Analysis::APIDumpAnalysis::QueryCommands::get_page_end(resource_id_vec.size(),
                                                                                  paging);
                ++n_item)
    {
        const uint64_t                             resource_id  = resource_id_vec.at(static_cast<size_t>(n_item) );
        const ResourceUse&                         use          = resource_use_map.at(resource_id);
        const Analysis::APIDumpAnalysis::Resource* resource_ptr = find_resource_ptr(in_timeline,
                                                                                    resource_id);

        inout_writer_ptr->begin_object();

        if (resource_ptr != nullptr)
        {
            write_resource_head(*resource_ptr,
                                resource_id,
                                inout_writer_ptr);
        }
        else
        {
            inout_writer_ptr->write_key("resource_id");
            inout_writer_ptr->write_u64(resource_id);
        }

        write_event_counts(use.n_event_array,
                           inout_writer_ptr);

        inout_writer_ptr->write_key   ("n_levels");
        inout_writer_ptr->write_u64   (use.level_set.size() );
        inout_writer_ptr->write_key   ("first_frame_index");
        inout_writer_ptr->write_u64   (use.first_frame);
        inout_writer_ptr->write_key   ("last_frame_index");
        inout_writer_ptr->write_u64   (use.last_frame);
        inout_writer_ptr->end_object  ();
    }

    inout_writer_ptr->end_array   ();
    inout_writer_ptr->end_object  ();
    inout_writer_ptr->end_object  ();

    return QueryStatus::OK;
}

void Analysis::APIDumpTextures::QueryCommands::write_blob(const TextureTimeline&   in_timeline,
                                                          const uint32_t&          in_blob_index,
                                                          Utils::Json::JsonWriter* inout_writer_ptr)
{
    inout_writer_ptr->write_key("blob");

    if (in_blob_index == NO_BLOB ||
        in_blob_index >= in_timeline.blob_vec.size() )
    {
        inout_writer_ptr->write_null();
    }
    else
    {
        const BlobEntry& blob = in_timeline.blob_vec.at(in_blob_index);

        inout_writer_ptr->begin_object();
        inout_writer_ptr->write_key   ("index");
        inout_writer_ptr->write_u64   (in_blob_index);
        inout_writer_ptr->write_key   ("location");
        inout_writer_ptr->write_string((blob.location == BlobLocation::WORKLOAD_CHUNK) ? "workload_chunk"
                                                                                       : "stored");

        if (blob.location == BlobLocation::WORKLOAD_CHUNK)
        {
            inout_writer_ptr->write_key("data_chunk_id");
            inout_writer_ptr->write_u64(blob.data_chunk_id);
        }
        else
        {
            inout_writer_ptr->write_key("file_offset_n_bytes");
            inout_writer_ptr->write_u64(blob.file_offset_n_bytes);
            inout_writer_ptr->write_key("n_stored_bytes");
            inout_writer_ptr->write_u64(blob.n_stored_bytes);
        }

        inout_writer_ptr->write_key   ("n_raw_bytes");
        inout_writer_ptr->write_u64   (blob.n_raw_bytes);
        inout_writer_ptr->write_key   ("content_hash");
        inout_writer_ptr->write_hex_u64(blob.content_hash);
        inout_writer_ptr->end_object  ();
    }
}

void Analysis::APIDumpTextures::QueryCommands::write_event(const TextureTimeline&   in_timeline,
                                                           const uint32_t&          in_event_index,
                                                           Utils::Json::JsonWriter* inout_writer_ptr)
{
    const TextureEvent& event = in_timeline.event_vec.at(in_event_index);

    inout_writer_ptr->write_key   ("event_index");
    inout_writer_ptr->write_u64   (in_event_index);
    inout_writer_ptr->write_key   ("kind");
    inout_writer_ptr->write_string(get_event_kind_name(event.kind) );
    inout_writer_ptr->write_key   ("frame_index");
    inout_writer_ptr->write_u64   (event.frame_index);
    inout_writer_ptr->write_key   ("call_index");
    inout_writer_ptr->write_u64   (event.call_index);
    inout_writer_ptr->write_key   ("level");
    inout_writer_ptr->write_u64   (event.destination.mip_level);

    inout_writer_ptr->write_key   ("first_layer");
    inout_writer_ptr->write_u64   (event.destination.layer);
    inout_writer_ptr->write_key   ("layer_count");
    inout_writer_ptr->write_u64   (event.layer_count);
    inout_writer_ptr->write_key   ("x");
    inout_writer_ptr->write_u64   (event.x);
    inout_writer_ptr->write_key   ("y");
    inout_writer_ptr->write_u64   (event.y);
    inout_writer_ptr->write_key   ("width");
    inout_writer_ptr->write_u64   (event.width);
    inout_writer_ptr->write_key   ("height");
    inout_writer_ptr->write_u64   (event.height);

    if (event.sample_index != 0)
    {
        inout_writer_ptr->write_key("sample");

        if (event.sample_index == ALL_SAMPLES)
        {
            inout_writer_ptr->write_string("all");
        }
        else
        {
            inout_writer_ptr->write_u64(event.sample_index);
        }
    }

    if (event.kind == TextureEventKind::RENDER_WRITE)
    {
        inout_writer_ptr->write_key("source");

        if (event.source.resource_id == Analysis::APIDumpAnalysis::ResourceId::NONE)
        {
            inout_writer_ptr->write_string("window");
        }
        else
        {
            inout_writer_ptr->write_u64(Analysis::APIDumpAnalysis::get_resource_id_value(event.source.resource_id) );
        }
    }

    write_blob(in_timeline,
               event.blob_index,
               inout_writer_ptr);
}

void Analysis::APIDumpTextures::QueryCommands::write_event_counts(const std::array<uint64_t, N_EVENT_KINDS>& in_n_event_array,
                                                                  Utils::Json::JsonWriter*                   inout_writer_ptr)
{
    inout_writer_ptr->write_key("n_uploads");
    inout_writer_ptr->write_u64(in_n_event_array.at(static_cast<size_t>(Analysis::APIDumpTextures::TextureEventKind::UPLOAD) ) );
    inout_writer_ptr->write_key("n_render_writes");
    inout_writer_ptr->write_u64(in_n_event_array.at(static_cast<size_t>(Analysis::APIDumpTextures::TextureEventKind::RENDER_WRITE) ) );
    inout_writer_ptr->write_key("n_generated_levels");
    inout_writer_ptr->write_u64(in_n_event_array.at(static_cast<size_t>(Analysis::APIDumpTextures::TextureEventKind::GENERATE_MIPS) ) );
    inout_writer_ptr->write_key("n_snapshots");
    inout_writer_ptr->write_u64(in_n_event_array.at(static_cast<size_t>(Analysis::APIDumpTextures::TextureEventKind::SNAPSHOT) ) );
}

void Analysis::APIDumpTextures::QueryCommands::write_level_size(const uint32_t&                       in_width,
                                                                const uint32_t&                       in_height,
                                                                const uint32_t&                       in_depth_or_layer_count,
                                                                const Utils::Graphics::TextureFormat& in_format,
                                                                Utils::Json::JsonWriter*              inout_writer_ptr)
{
    inout_writer_ptr->write_key   ("width");
    inout_writer_ptr->write_u64   (in_width);
    inout_writer_ptr->write_key   ("height");
    inout_writer_ptr->write_u64   (in_height);
    inout_writer_ptr->write_key   ("depth_or_layer_count");
    inout_writer_ptr->write_u64   (in_depth_or_layer_count);
    inout_writer_ptr->write_key   ("format");
    inout_writer_ptr->write_string(Utils::Graphics::TextureFormats::get_info(in_format).name_ptr);
}

void Analysis::APIDumpTextures::QueryCommands::write_resource_head(const Analysis::APIDumpAnalysis::Resource& in_resource,
                                                                   const uint64_t&                            in_resource_id,
                                                                   Utils::Json::JsonWriter*                   inout_writer_ptr)
{
    inout_writer_ptr->write_key   ("resource_id");
    inout_writer_ptr->write_u64   (in_resource_id);
    inout_writer_ptr->write_key   ("name");
    inout_writer_ptr->write_string(in_resource.debug_name);
    inout_writer_ptr->write_key   ("kind");
    inout_writer_ptr->write_string(Analysis::APIDumpAnalysis::QueryCommands::get_resource_kind_name(in_resource.description.kind) );
    inout_writer_ptr->write_key   ("width");
    inout_writer_ptr->write_u64   (in_resource.description.width);
    inout_writer_ptr->write_key   ("height");
    inout_writer_ptr->write_u64   (in_resource.description.height);
}
