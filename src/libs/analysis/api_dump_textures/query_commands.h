/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Queries against a .textures file and the JSON they return (see APIDumpTextureQuery):
 *
 *   info       what the file describes, event counts per kind, where the contents are stored
 *   resources  every texture with events: event counts per kind and levels (paged)
 *   resource   one texture: its levels and its events in call order (paged)
 *   at         one level at the end of a frame (or at a call): which event made each part, where the bytes are, the last snapshot
 *
 * Frames are numbered from 0, resources from 1. Keys are hex strings.
 */
#ifndef APIDUMPTEXTURES_QUERY_COMMANDS_H
#define APIDUMPTEXTURES_QUERY_COMMANDS_H

#include "analysis/api_dump_analysis/types.h"
#include "analysis/api_dump_textures/types.h"
#include "utils/json/json_writer.h"
#include <array>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace Analysis
{
    namespace APIDumpTextures
    {
        /* Query arguments; each command uses only some. */
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
            static const std::vector<std::string>& get_command_names();

            /* Runs a command and writes JSON to the writer. If the status is not OK, the error is set and the writer content must be discarded. */
            static Analysis::APIDumpAnalysis::QueryStatus run(const std::string&       in_command,
                                                              const TextureTimeline&   in_timeline,
                                                              const QueryArguments&    in_arguments,
                                                              Utils::Json::JsonWriter* inout_writer_ptr,
                                                              std::string*             out_error_ptr);

        private:
            typedef Analysis::APIDumpAnalysis::QueryStatus QueryStatus;

            /* Number of event kinds; size of the event count arrays. */
            static constexpr size_t N_EVENT_KINDS = 4;

            /* Event summary of one texture, for the lists. */
            struct ResourceUse
            {
                uint64_t                            first_frame   = UINT64_MAX;
                uint64_t                            last_frame    = 0;
                std::set<uint32_t>                  level_set;
                std::array<uint64_t, N_EVENT_KINDS> n_event_array = {};
            };

            /* Returns the resource with the id (from 1), or nullptr. */
            static const Analysis::APIDumpAnalysis::Resource* find_resource_ptr(const TextureTimeline& in_timeline,
                                                                                const uint64_t&        in_resource_id);

            static const char* get_event_kind_name(const TextureEventKind& in_kind);

            /* Returns limit and offset in the form the other analysis queries use. */
            static Analysis::APIDumpAnalysis::QueryArguments get_paging(const QueryArguments& in_arguments);

            /* Returns the event summary of every texture with events, keyed by resource id. */
            static std::map<uint64_t, ResourceUse> get_resource_use_map(const TextureTimeline& in_timeline);

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

            /* Writes where a content is (workload chunk, or file of blobs offset and sizes); null for an event without content. */
            static void write_blob(const TextureTimeline&   in_timeline,
                                   const uint32_t&          in_blob_index,
                                   Utils::Json::JsonWriter* inout_writer_ptr);

            /* Writes an event as members of the open JSON object. */
            static void write_event(const TextureTimeline&   in_timeline,
                                    const uint32_t&          in_event_index,
                                    Utils::Json::JsonWriter* inout_writer_ptr);

            static void write_event_counts(const std::array<uint64_t, N_EVENT_KINDS>& in_n_event_array,
                                           Utils::Json::JsonWriter*                   inout_writer_ptr);

            /* Writes the size and format of a level as members of the open JSON object. */
            static void write_level_size(const uint32_t&                       in_width,
                                         const uint32_t&                       in_height,
                                         const uint32_t&                       in_depth_or_layer_count,
                                         const Utils::Graphics::TextureFormat& in_format,
                                         Utils::Json::JsonWriter*              inout_writer_ptr);

            /* Writes what a resource is as members of the open JSON object. */
            static void write_resource_head(const Analysis::APIDumpAnalysis::Resource& in_resource,
                                            const uint64_t&                            in_resource_id,
                                            Utils::Json::JsonWriter*                   inout_writer_ptr);
        };
    }
}

#endif /* APIDUMPTEXTURES_QUERY_COMMANDS_H */
