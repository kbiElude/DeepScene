/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Queries over an analysis. Each command writes its JSON answer to a JsonWriter and does no I/O.
 *
 * Commands:
 *   depth      draw depth per pass and frame kind: the depths below which 10%, 50% and 90% of the draws lie
 *   info       what the analysis is of, and how much it found
 *   timeline   runs of frames of one shape, in time order (paged)
 *   pipelines  frame shapes and their pass structures (paged)
 *   overlays   passes with a constant camera and draws that are flat layers (an orthographic projection, or all vertices at one depth through
 *              an identity projection): 2D overlay candidates, as a recipe "unaffected" list (a hint only)
 *   pass       one pass structure, with its draws (paged) and camera constants
 *   frame      one frame by index, or a representative frame of a shape
 *   resources  what the frames did with each resource, optionally for one class (paged)
 *
 * Times are in seconds since the first call of the workload. Keys are hex strings.
 */
#ifndef APIDUMPANALYSIS_QUERY_COMMANDS_H
#define APIDUMPANALYSIS_QUERY_COMMANDS_H

#include "analysis/api_dump_analysis/model.h"
#include "analysis/api_dump_analysis/types.h"
#include "utils/json/json_writer.h"
#include <string>
#include <vector>

namespace Analysis
{
    namespace APIDumpAnalysis
    {
        class QueryCommands
        {
        public:
            /* The names of the commands. */
            static const std::vector<std::string>& get_command_names();

            /* Helpers shared by the queries: a resource kind name, and the end of a list page (offset up to the limit; limit 0 means all). */
            static uint64_t    get_page_end          (const uint64_t&       in_total,
                                                      const QueryArguments& in_arguments);
            static const char* get_resource_kind_name(const ResourceKind&   in_kind);

            /* Overlays data for tools: the first frame containing the pass structure (false if none), a pass's vertex depth spread, and the
             * passes with a constant camera (in ID order).
             */
            static bool get_first_frame_index(const AnalysisResult& in_analysis,
                                              const uint32_t&       in_pass_id,
                                              uint64_t*             out_frame_index_ptr);
            /* The frames that contain the pass structure, in time order. */
            static std::vector<uint64_t> get_frame_index_vec          (const AnalysisResult& in_analysis,
                                                                       const uint32_t&       in_pass_id);
            static PassDepthSpread       get_pass_depth_spread        (const AnalysisResult& in_analysis,
                                                                       const uint32_t&       in_pass_id);
            static std::vector<uint32_t> get_static_camera_pass_id_vec(const AnalysisResult& in_analysis);

            /* Whether the draws of a draw structure that were measured are all flat layers: in the space of the picture, in pixels, with an orthographic projection or
             * through an identity projection (see DrawFacts::n_draws_flat_layer). False if none was measured.
             */
            static bool is_flat_layer_structure(const AnalysisResult& in_analysis,
                                                const uint64_t&       in_structure_key);

            /* Reads a class name: frame_local, loop_carried, static, unread or unused. */
            static bool parse_resource_access_class(const std::string&   in_name,
                                                    ResourceAccessClass* out_class_ptr);

            /* Runs a command. Sets the error unless it returns OK; the text in the writer is unusable then. */
            static QueryStatus run(const std::string&       in_command,
                                   const AnalysisResult&    in_analysis,
                                   const QueryArguments&    in_arguments,
                                   Utils::Json::JsonWriter* inout_writer_ptr,
                                   std::string*             out_error_ptr);

            /* Writes the "depth" member of an object: the depth spread of the vertices of a pass, or that it is unknown. */
            static void write_depth_spread(const PassDepthSpread&   in_spread,
                                           Utils::Json::JsonWriter* inout_writer_ptr);

            /* Writes the "facts" member of an object: what the draws of a structure did (see DrawFacts). */
            static void write_draw_facts  (const DrawFacts&         in_facts,
                                           Utils::Json::JsonWriter* inout_writer_ptr);
            static void write_page        (const uint64_t&          in_total,
                                           const QueryArguments&    in_arguments,
                                           Utils::Json::JsonWriter* inout_writer_ptr);

        private:
            static const char* get_constant_block_kind_name  (const ConstantBlockKind&    in_kind);
            static const char* get_constant_provenance_name  (const ConstantProvenance&   in_provenance);
            static const char* get_constant_semantic_name    (const ConstantSemantic&     in_semantic);
            static std::string get_hex_string                (const std::vector<uint8_t>& in_byte_vec);
            static const char* get_primitive_topology_name   (const PrimitiveTopology&    in_topology);
            static const char* get_resource_access_class_name(const ResourceAccessClass&  in_class);
            static double      get_seconds                   (const AnalysisResult&       in_analysis,
                                                              const uint64_t&             in_timestamp_qpc);

            static const char* get_transfer_kind_name(const TransferKind& in_kind);

            static QueryStatus run_depth    (const AnalysisResult&    in_analysis,
                                             Utils::Json::JsonWriter* inout_writer_ptr);
            static QueryStatus run_frame    (const AnalysisResult&    in_analysis,
                                             const QueryArguments&    in_arguments,
                                             Utils::Json::JsonWriter* inout_writer_ptr,
                                             std::string*             out_error_ptr);
            static QueryStatus run_info     (const AnalysisResult&    in_analysis,
                                             Utils::Json::JsonWriter* inout_writer_ptr);
            static QueryStatus run_overlays (const AnalysisResult&    in_analysis,
                                             Utils::Json::JsonWriter* inout_writer_ptr);
            static QueryStatus run_pass     (const AnalysisResult&    in_analysis,
                                             const QueryArguments&    in_arguments,
                                             Utils::Json::JsonWriter* inout_writer_ptr,
                                             std::string*             out_error_ptr);
            static QueryStatus run_pipelines(const AnalysisResult&    in_analysis,
                                             const QueryArguments&    in_arguments,
                                             Utils::Json::JsonWriter* inout_writer_ptr);
            static QueryStatus run_resources(const AnalysisResult&    in_analysis,
                                             const QueryArguments&    in_arguments,
                                             Utils::Json::JsonWriter* inout_writer_ptr);
            static QueryStatus run_timeline (const AnalysisResult&    in_analysis,
                                             const QueryArguments&    in_arguments,
                                             Utils::Json::JsonWriter* inout_writer_ptr);

            static void write_draw_structure(const uint64_t&                   in_key,
                                             const DrawStructure&              in_structure,
                                             Utils::Json::JsonWriter*          inout_writer_ptr);
            static void write_frame         (const AnalysisResult&             in_analysis,
                                             const uint64_t&                   in_frame_index,
                                             const RepresentativeStore::Entry* in_entry_ptr,
                                             Utils::Json::JsonWriter*          inout_writer_ptr);
            static void write_resource_view (const AnalysisResult&             in_analysis,
                                             const ResourceView&               in_view,
                                             Utils::Json::JsonWriter*          inout_writer_ptr);
            static void write_series        (const AnalysisResult&             in_analysis,
                                             const ValueTimeSeries&            in_series,
                                             Utils::Json::JsonWriter*          inout_writer_ptr);
            static void write_sample        (const AnalysisResult&             in_analysis,
                                             const ValueTimeSeries::Sample&    in_sample,
                                             Utils::Json::JsonWriter*          inout_writer_ptr);
        };
    }
}

#endif /* APIDUMPANALYSIS_QUERY_COMMANDS_H */
