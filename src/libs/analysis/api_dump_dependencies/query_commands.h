/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * The queries that can be run against a .deps file, and the JSON they return (see APIDumpDependencyQuery):
 *
 *   info       what the file describes, and how many resources have each dependency class
 *   resources  every resource with its totals, optionally limited to one dependency class (paged)
 *   resource   one resource: its totals, the runs of frames that use it in the same way, and the versions of its content with the frames that use them (paged)
 *   frame      one frame: the calls it spans, and what it does with each resource it touches
 *   carries    what a frame uses of the content that earlier frames left, and which frame left it
 *   per-eye    the resources that are rendered to and carry content from frame to frame: the ones each eye needs a copy of, which a recipe lists
 *              as its "per_eye_textures"
 *   slice      the frames that a replay has to make to reach a frame (see FrameSlicePlanner), and the runs of frames it can leave out
 *
 * Frames are numbered from 0, as in the other files of the analysis. Keys are hex strings.
 */
#ifndef APIDUMPDEPENDENCIES_QUERY_COMMANDS_H
#define APIDUMPDEPENDENCIES_QUERY_COMMANDS_H

#include "analysis/api_dump_analysis/types.h"
#include "analysis/api_dump_dependencies/types.h"
#include "utils/json/json_writer.h"
#include <string>
#include <vector>

namespace Analysis
{
    namespace APIDumpDependencies
    {
        /* The arguments of a query. Each command uses only some of them. */
        struct QueryArguments
        {
            DependencyClass dependency_class = DependencyClass::CARRIED;
            uint64_t        frame_index      = 0;
            bool            has_class        = false;
            bool            has_frame_index  = false;
            bool            has_resource_id  = false;
            uint64_t        limit            = 100; /* items per list; 0 is all */
            uint64_t        offset           = 0;   /* items to skip            */
            uint64_t        resource_id      = 0;
        };

        class QueryCommands
        {
        public:
            /* The command names. */
            static const std::vector<std::string>& get_command_names();

            /* Parses a dependency class name (carried, carried_possibly, frame_local, static or unused). Returns false if the name is unknown. */
            static bool parse_dependency_class(const std::string& in_name,
                                               DependencyClass*   out_class_ptr);

            /* Runs a command and writes the JSON result to the writer. If the status is not OK, the error is set and the content of the writer
             * must be discarded.
             */
            static Analysis::APIDumpAnalysis::QueryStatus run(const std::string&           in_command,
                                                              const DependencyFileContent& in_analysis,
                                                              const QueryArguments&        in_arguments,
                                                              Utils::Json::JsonWriter*     inout_writer_ptr,
                                                              std::string*                 out_error_ptr);

        private:
            /* Returns the version whose content the frame uses: the latest version written before the frame, or the version without a writer.
             * Returns nullptr if there is none.
             */
            static const ResourceVersion* find_consumed_version_ptr(const ResourceDependencies& in_dependencies,
                                                                    const uint64_t&             in_frame_index);

            /* Returns the use run that contains the frame, or nullptr if the frame does not touch the resource. */
            static const UseRun* find_use_run_ptr(const ResourceDependencies& in_dependencies,
                                                  const uint64_t&             in_frame_index);

            static const char* get_carry_kind_name      (const CarryKind&       in_kind);
            static const char* get_dependency_class_name(const DependencyClass& in_class);

            /* Returns the paging arguments (limit and offset) in the form that the other query files of the analysis use. */
            static Analysis::APIDumpAnalysis::QueryArguments get_paging(const QueryArguments& in_arguments);

            static Analysis::APIDumpAnalysis::QueryStatus run_carries  (const DependencyFileContent& in_analysis,
                                                                        const QueryArguments&        in_arguments,
                                                                        Utils::Json::JsonWriter*     inout_writer_ptr,
                                                                        std::string*                 out_error_ptr);
            static Analysis::APIDumpAnalysis::QueryStatus run_frame    (const DependencyFileContent& in_analysis,
                                                                        const QueryArguments&        in_arguments,
                                                                        const bool&                  in_is_carries_only,
                                                                        Utils::Json::JsonWriter*     inout_writer_ptr,
                                                                        std::string*                 out_error_ptr);
            static Analysis::APIDumpAnalysis::QueryStatus run_info     (const DependencyFileContent& in_analysis,
                                                                        Utils::Json::JsonWriter*     inout_writer_ptr);
            static Analysis::APIDumpAnalysis::QueryStatus run_per_eye  (const DependencyFileContent& in_analysis,
                                                                        Utils::Json::JsonWriter*     inout_writer_ptr);
            static Analysis::APIDumpAnalysis::QueryStatus run_resource (const DependencyFileContent& in_analysis,
                                                                        const QueryArguments&        in_arguments,
                                                                        Utils::Json::JsonWriter*     inout_writer_ptr,
                                                                        std::string*                 out_error_ptr);
            static Analysis::APIDumpAnalysis::QueryStatus run_resources(const DependencyFileContent& in_analysis,
                                                                        const QueryArguments&        in_arguments,
                                                                        Utils::Json::JsonWriter*     inout_writer_ptr);
            static Analysis::APIDumpAnalysis::QueryStatus run_slice    (const DependencyFileContent& in_analysis,
                                                                        const QueryArguments&        in_arguments,
                                                                        Utils::Json::JsonWriter*     inout_writer_ptr,
                                                                        std::string*                 out_error_ptr);

            /* write_resource_head writes what the resource is, and write_summary writes its totals. */
            static void write_resource_head(const DependencyFileContent& in_analysis,
                                            const size_t&                in_n_resource,
                                            Utils::Json::JsonWriter*     inout_writer_ptr);
            static void write_summary      (const ResourceSummary&    in_summary,
                                            Utils::Json::JsonWriter*  inout_writer_ptr);
        };
    }
}

#endif /* APIDUMPDEPENDENCIES_QUERY_COMMANDS_H */
