/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * The questions that a .textures file is asked, and the JSON they are answered with (see APIDumpTextureQuery):
 *
 *   info       what the file is of, how many events of each kind it has, and where the contents are
 *   resources  every texture that has events, with how many of each kind and the levels it has (paged)
 *   resource   one texture: its levels, and its events in the order of the calls (paged)
 *   at         what one level of a texture holds at the end of a frame (or at a call): which event made each part of it, where the bytes are, and the last snapshot
 *
 * Frames are numbered from 0, as in the other files of the analysis; a resource from 1. Keys are hex strings.
 */
#ifndef APIDUMPTEXTURES_QUERY_COMMANDS_H
#define APIDUMPTEXTURES_QUERY_COMMANDS_H

#include "analysis/api_dump_analysis/types.h"
#include "analysis/api_dump_textures/types.h"
#include "utils/json/json_writer.h"
#include <string>
#include <vector>

namespace Analysis
{
    namespace APIDumpTextures
    {
        /* What a query is about. Each command uses only some of the arguments. */
        struct QueryArguments
        {
            uint64_t call_limit       = 0;
            uint64_t frame_index      = 0;
            bool     has_call_limit   = false;
            bool     has_frame_index  = false;
            bool     has_mip_level    = false;
            bool     has_resource_id  = false;
            uint64_t limit            = 100; /* items per list; 0 is all */
            uint64_t mip_level        = 0;
            uint64_t offset           = 0;   /* items to skip            */
            uint64_t resource_id      = 0;
        };

        class QueryCommands
        {
        public:
            /* The command names. */
            static const std::vector<std::string>& get_command_names();

            /* Runs a command. Sets the error unless it returns OK; the writer's text is unusable then. */
            static Analysis::APIDumpAnalysis::QueryStatus run(const std::string&       in_command,
                                                              const TextureTimeline&   in_timeline,
                                                              const QueryArguments&    in_arguments,
                                                              Utils::Json::JsonWriter* inout_writer_ptr,
                                                              std::string*             out_error_ptr);

        private:
            /* The resource with the id (from 1): nullptr if the file has none. */
            static const Analysis::APIDumpAnalysis::Resource* find_resource_ptr(const TextureTimeline& in_timeline,
                                                                                const uint64_t&        in_resource_id);

            static const char* get_event_kind_name(const TextureEventKind& in_kind);

            /* The paging of a list: the arguments of the other files of the analysis. */
            static Analysis::APIDumpAnalysis::QueryArguments get_paging(const QueryArguments& in_arguments);

            static Analysis::APIDumpAnalysis::QueryStatus run_at       (const TextureTimeline&   in_timeline,
                                                                        const QueryArguments&    in_arguments,
                                                                        Utils::Json::JsonWriter* inout_writer_ptr,
                                                                        std::string*             out_error_ptr);
            static Analysis::APIDumpAnalysis::QueryStatus run_info     (const TextureTimeline&   in_timeline,
                                                                        Utils::Json::JsonWriter* inout_writer_ptr);
            static Analysis::APIDumpAnalysis::QueryStatus run_resource (const TextureTimeline&   in_timeline,
                                                                        const QueryArguments&    in_arguments,
                                                                        Utils::Json::JsonWriter* inout_writer_ptr,
                                                                        std::string*             out_error_ptr);
            static Analysis::APIDumpAnalysis::QueryStatus run_resources(const TextureTimeline&   in_timeline,
                                                                        const QueryArguments&    in_arguments,
                                                                        Utils::Json::JsonWriter* inout_writer_ptr);

            /* Where the bytes of a content are: in the workload (the chunk), or in the file of blobs (the offset and the sizes); null for an event that has none. */
            static void write_blob(const TextureTimeline&   in_timeline,
                                   const uint32_t&          in_blob_index,
                                   Utils::Json::JsonWriter* inout_writer_ptr);

            /* An event, in the members of an object that is open. */
            static void write_event(const TextureTimeline&   in_timeline,
                                    const uint32_t&          in_event_index,
                                    Utils::Json::JsonWriter* inout_writer_ptr);

            /* The size and the format of a level, in the members of an object that is open. */
            static void write_level_size(const uint32_t&                      in_width,
                                         const uint32_t&                      in_height,
                                         const uint32_t&                      in_depth_or_layer_count,
                                         const Utils::Graphics::TextureFormat& in_format,
                                         Utils::Json::JsonWriter*             inout_writer_ptr);

            /* A resource: what it is, in the members of an object that is open. */
            static void write_resource_head(const Analysis::APIDumpAnalysis::Resource& in_resource,
                                            const uint64_t&                            in_resource_id,
                                            Utils::Json::JsonWriter*                   inout_writer_ptr);
        };
    }
}

#endif /* APIDUMPTEXTURES_QUERY_COMMANDS_H */
