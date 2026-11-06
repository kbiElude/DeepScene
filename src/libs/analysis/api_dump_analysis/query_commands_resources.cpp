/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_analysis/query_commands.h"
#include "analysis/api_dump_analysis/resource_access_tracker.h"
#include "analysis/api_dump_analysis/resource_identity.h"
#include "common/macros.h"
#include <algorithm>

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

Analysis::APIDumpAnalysis::QueryStatus Analysis::APIDumpAnalysis::QueryCommands::run_resources(const AnalysisResult&    in_analysis,
                                                                                               const QueryArguments&    in_arguments,
                                                                                               Utils::Json::JsonWriter* inout_writer_ptr)
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
        const size_t     n_resource  = match_vec.at                    (static_cast<size_t>(n_match) );
        const auto&      access      = model_ref.resource_access_vec.at(n_resource);
        const auto&      resource    = model_ref.resource_vec.at       (n_resource);
        const auto       resource_id = make_resource_id(static_cast<uint32_t>(n_resource + 1) );
        ResourceIdentity identity;

        ResourceIdentities::find(model_ref.resource_vec,
                                 resource_id,
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
