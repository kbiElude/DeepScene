/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * The questions that a .deps file is asked, and the JSON they are answered with (see APIDumpDependencyQuery):
 *
 *   info       what the file is of, and how many resources are of each kind of dependency
 *   resources  every resource with its totals, optionally of one class of dependency (paged)
 *   resource   one resource: its totals, the runs of frames which use it alike, and the versions of its content with their consumers (paged)
 *   frame      one frame: the calls it spans, and what it does with each resource it touches
 *   carries    what a frame uses of what the frames before it left, and which frame left it
 *   per-eye    the resources which carry content from frame to frame and are rendered to: those which each eye needs a copy of, as a recipe
 *              names them (see the "per_eye_textures" of a recipe)
 *   slice      the frames whose content a replay has to make to go to a frame (see FrameSlicePlanner), and the runs of frames it can leave out
 *
 * Frames are numbered from 0, as in the other files of the analysis. Keys are hex strings.
 */
#ifndef APIDUMPDEPENDENCIES_QUERY_COMMANDS_H
#define APIDUMPDEPENDENCIES_QUERY_COMMANDS_H

#include "analysis/api_dump_analysis/json_writer.h"
#include "analysis/api_dump_analysis/types.h"
#include "analysis/api_dump_dependencies/types.h"
#include <string>
#include <vector>

namespace Analysis
{
    namespace APIDumpDependencies
    {
        /* What a query is about. Each command uses only some of the arguments. */
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

            /* Reads a class name: carried, carried_possibly, frame_local, static or unused. */
            static bool parse_dependency_class(const std::string& in_name,
                                               DependencyClass*   out_class_ptr);

            /* Runs a command. Sets the error unless it returns OK; the writer's text is unusable then. */
            static Analysis::APIDumpAnalysis::QueryStatus run(const std::string&        in_command,
                                                              const DependencyAnalysis& in_analysis,
                                                              const QueryArguments&     in_arguments,
                                                              Analysis::APIDumpAnalysis::JsonWriter* inout_writer_ptr,
                                                              std::string*              out_error_ptr);

        private:
            static const char* get_carry_kind_name       (const CarryKind&       in_kind);
            static const char* get_dependency_class_name (const DependencyClass& in_class);

            /* The paging of a list: the arguments of the other files of the analysis. */
            static Analysis::APIDumpAnalysis::QueryArguments get_paging(const QueryArguments& in_arguments);

            /* The version of a resource whose content a frame uses from the frames before: nullptr if there is none. */
            static const ResourceVersion* find_consumed_version_ptr(const ResourceDependencies& in_dependencies,
                                                                    const uint64_t&             in_frame_index);

            /* What a frame does with a resource: nullptr if it does not touch it. */
            static const UseRun* find_use_run_ptr(const ResourceDependencies& in_dependencies,
                                                  const uint64_t&             in_frame_index);

            static Analysis::APIDumpAnalysis::QueryStatus run_carries  (const DependencyAnalysis& in_analysis,
                                                                        const QueryArguments&     in_arguments,
                                                                        Analysis::APIDumpAnalysis::JsonWriter* inout_writer_ptr,
                                                                        std::string*              out_error_ptr);
            static Analysis::APIDumpAnalysis::QueryStatus run_frame    (const DependencyAnalysis& in_analysis,
                                                                        const QueryArguments&     in_arguments,
                                                                        const bool&               in_is_carries_only,
                                                                        Analysis::APIDumpAnalysis::JsonWriter* inout_writer_ptr,
                                                                        std::string*              out_error_ptr);
            static Analysis::APIDumpAnalysis::QueryStatus run_info     (const DependencyAnalysis& in_analysis,
                                                                        Analysis::APIDumpAnalysis::JsonWriter* inout_writer_ptr);
            static Analysis::APIDumpAnalysis::QueryStatus run_per_eye  (const DependencyAnalysis& in_analysis,
                                                                        Analysis::APIDumpAnalysis::JsonWriter* inout_writer_ptr);
            static Analysis::APIDumpAnalysis::QueryStatus run_resource (const DependencyAnalysis& in_analysis,
                                                                        const QueryArguments&     in_arguments,
                                                                        Analysis::APIDumpAnalysis::JsonWriter* inout_writer_ptr,
                                                                        std::string*              out_error_ptr);
            static Analysis::APIDumpAnalysis::QueryStatus run_resources(const DependencyAnalysis& in_analysis,
                                                                        const QueryArguments&     in_arguments,
                                                                        Analysis::APIDumpAnalysis::JsonWriter* inout_writer_ptr);
            static Analysis::APIDumpAnalysis::QueryStatus run_slice    (const DependencyAnalysis& in_analysis,
                                                                        const QueryArguments&     in_arguments,
                                                                        Analysis::APIDumpAnalysis::JsonWriter* inout_writer_ptr,
                                                                        std::string*              out_error_ptr);

            /* A resource: what it is, and its totals. */
            static void write_resource_head(const DependencyAnalysis&                 in_analysis,
                                            const size_t&                             in_n_resource,
                                            Analysis::APIDumpAnalysis::JsonWriter*    inout_writer_ptr);
            static void write_summary      (const ResourceSummary&                    in_summary,
                                            Analysis::APIDumpAnalysis::JsonWriter*    inout_writer_ptr);
        };
    }
}

#endif /* APIDUMPDEPENDENCIES_QUERY_COMMANDS_H */
