/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Queries over an analysis. Each command writes its JSON answer to a JsonWriter and does no I/O, so the tool and the
 * tests can use it freely.
 *
 * Commands:
 *   info       what the analysis is of, and how much it found
 *   timeline   runs of frames of one shape, in time order (paged)
 *   pipelines  frame shapes and the pass structures each is made of (paged)
 *   overlays   passes whose camera never changes, and the draws of them: candidates for 2D overlays, as a recipe "unaffected" list (a hint only)
 *   pass       one pass structure, with its draws (paged) and camera constants
 *   frame      one frame by index, or a representative frame of a shape
 *   resources  what the frames did with each resource, optionally of one class (paged)
 *
 * Times are in seconds since the first call of the workload. Keys are hex strings.
 */
#ifndef APIDUMPANALYSIS_QUERY_COMMANDS_H
#define APIDUMPANALYSIS_QUERY_COMMANDS_H

#include "Analysis/APIDumpAnalysis/json_writer.h"
#include "Analysis/APIDumpAnalysis/model.h"
#include "Analysis/APIDumpAnalysis/types.h"
#include <string>
#include <vector>

namespace Analysis
{
    namespace APIDumpAnalysis
    {
        class QueryCommands
        {
        public:
            /* The command names. */
            static const std::vector<std::string>& get_command_names();

            /* What the queries of every file of the analysis say alike: the name of a kind of resource, and the page of a list (the items from the
             * offset up to the limit of the arguments; 0 is all, which the end of the page is the total for).
             */
            static uint64_t    get_page_end          (const uint64_t&       in_total,
                                                      const QueryArguments& in_arguments);
            static const char* get_resource_kind_name(const ResourceKind&   in_kind);
            static void        write_page            (const uint64_t&       in_total,
                                                      const QueryArguments& in_arguments,
                                                      JsonWriter*           inout_writer_ptr);

            /* Reads a class name: frame_local, loop_carried, static, unread or unused. */
            static bool parse_resource_access_class(const std::string&   in_name,
                                                    ResourceAccessClass* out_class_ptr);

            /* Runs a command. Sets the error unless it returns OK; the writer's text is unusable then. */
            static QueryStatus run(const std::string&    in_command,
                                   const AnalysisResult& in_analysis,
                                   const QueryArguments& in_arguments,
                                   JsonWriter*           inout_writer_ptr,
                                   std::string*          out_error_ptr);

        private:
            static const char* get_constant_block_kind_name  (const ConstantBlockKind&    in_kind);
            static const char* get_constant_provenance_name  (const ConstantProvenance&   in_provenance);
            static const char* get_constant_semantic_name    (const ConstantSemantic&     in_semantic);
            static bool        get_first_frame_index         (const AnalysisResult&       in_analysis,
                                                              const uint32_t&             in_pass_id,
                                                              uint64_t*                   out_frame_index_ptr);
            static std::string get_hex_string                (const std::vector<uint8_t>& in_byte_vec);
            static const char* get_primitive_topology_name   (const PrimitiveTopology&    in_topology);
            static const char* get_resource_access_class_name(const ResourceAccessClass&  in_class);
            static double      get_seconds                  (const AnalysisResult&       in_analysis,
                                                              const uint64_t&             in_timestamp_qpc);

            static const char* get_transfer_kind_name(const TransferKind& in_kind);

            static QueryStatus run_depth    (const AnalysisResult& in_analysis,
                                             JsonWriter*           inout_writer_ptr);
            static QueryStatus run_frame    (const AnalysisResult& in_analysis,
                                             const QueryArguments& in_arguments,
                                             JsonWriter*           inout_writer_ptr,
                                             std::string*          out_error_ptr);
            static QueryStatus run_info     (const AnalysisResult& in_analysis,
                                             JsonWriter*           inout_writer_ptr);
            static QueryStatus run_overlays (const AnalysisResult& in_analysis,
                                             JsonWriter*           inout_writer_ptr);
            static QueryStatus run_pass     (const AnalysisResult& in_analysis,
                                             const QueryArguments& in_arguments,
                                             JsonWriter*           inout_writer_ptr,
                                             std::string*          out_error_ptr);
            static QueryStatus run_pipelines(const AnalysisResult& in_analysis,
                                             const QueryArguments& in_arguments,
                                             JsonWriter*           inout_writer_ptr);
            static QueryStatus run_resources(const AnalysisResult& in_analysis,
                                             const QueryArguments& in_arguments,
                                             JsonWriter*           inout_writer_ptr);
            static QueryStatus run_timeline (const AnalysisResult& in_analysis,
                                             const QueryArguments& in_arguments,
                                             JsonWriter*           inout_writer_ptr);

            static void write_draw_structure(const uint64_t&                   in_key,
                                             const DrawStructure&              in_structure,
                                             JsonWriter*                       inout_writer_ptr);
            static void write_frame         (const AnalysisResult&             in_analysis,
                                             const uint64_t&                   in_frame_index,
                                             const RepresentativeStore::Entry* in_entry_ptr,
                                             JsonWriter*                       inout_writer_ptr);
            static void write_resource_view (const AnalysisResult&             in_analysis,
                                             const ResourceView&               in_view,
                                             JsonWriter*                       inout_writer_ptr);
            static void write_series        (const AnalysisResult&             in_analysis,
                                             const ValueTimeSeries&            in_series,
                                             JsonWriter*                       inout_writer_ptr);
            static void write_sample        (const AnalysisResult&             in_analysis,
                                             const ValueTimeSeries::Sample&    in_sample,
                                             JsonWriter*                       inout_writer_ptr);
        };
    }
}

#endif /* APIDUMPANALYSIS_QUERY_COMMANDS_H */
