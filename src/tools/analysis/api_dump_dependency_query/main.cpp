/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_analysis/query_command_line.h"
#include "analysis/api_dump_analysis/query_program_runner.h"
#include "analysis/api_dump_dependencies/deps_file.h"
#include "query.h"
#include "utils/command_line/number_parser.h"
#include <stdio.h>
#include <stdlib.h>

bool Analysis::APIDumpDependencyQuery::parse_arguments(const int&   in_argc,
                                                       char*        in_argv[],
                                                       Options*     out_options_ptr,
                                                       std::string* out_error_ptr)
{
    typedef Analysis::APIDumpAnalysis::QueryCommandLine QueryCommandLine;

    Analysis::APIDumpAnalysis::QueryPositionalRules positional_rules;
    Analysis::APIDumpAnalysis::QueryPositionals     positionals;
    bool                                            result = false;

    /* The option of this tool in addition to those every query tool has: --class. */
    const QueryCommandLine::ProgramOptionFunc program_option_func = [&](const std::string& in_argument,
                                                                        const bool&        in_has_next,
                                                                        const std::string& in_next_argument,
                                                                        bool*              out_is_known_ptr,
                                                                        bool*              out_uses_next_ptr,
                                                                        std::string*       out_error_ptr_inner)
    {
        bool is_set = true;

        *out_is_known_ptr  = (in_argument == "--class");
        *out_uses_next_ptr = *out_is_known_ptr;

        if (*out_is_known_ptr == false)
        {
            /* Not an option of this tool. */
        }
        else
        if (in_has_next == false)
        {
            *out_error_ptr_inner = "--class needs carried, carried_possibly, frame_local, static or unused";
            is_set               = false;
        }
        else
        if (Analysis::APIDumpDependencies::QueryCommands::parse_dependency_class(in_next_argument,
                                                                                &out_options_ptr->arguments.dependency_class) == false)
        {
            *out_error_ptr_inner = "--class needs carried, carried_possibly, frame_local, static or unused, not " + in_next_argument;
            is_set               = false;
        }
        else
        {
            out_options_ptr->arguments.has_class = true;
        }

        return is_set;
    };

    /* The positional arguments: the file, the command, and then the id of a resource or the index of a frame. */
    positional_rules.max_n_numbers     = 1;
    positional_rules.missing_text      = "A .deps file and a command are needed";
    positional_rules.number_error_text = "An id must be a number, not ";

    if (QueryCommandLine::parse_file_command_and_numbers(in_argc,
                                                         in_argv,
                                                         std::vector<Analysis::APIDumpAnalysis::QueryNumberOption>(),
                                                         program_option_func,
                                                         positional_rules,
                                                        &out_options_ptr->common,
                                                        &positionals,
                                                         out_error_ptr) == false)
    {
        goto end;
    }

    out_options_ptr->command       = positionals.command;
    out_options_ptr->deps_filename = positionals.filename;

    if (positionals.number_vec.empty() == false)
    {
        const uint64_t value = positionals.number_vec.at(0);

        out_options_ptr->arguments.frame_index     = value;
        out_options_ptr->arguments.has_frame_index = (out_options_ptr->command == "frame"   ||
                                                      out_options_ptr->command == "carries" ||
                                                      out_options_ptr->command == "slice");
        out_options_ptr->arguments.has_resource_id = (out_options_ptr->command == "resource");
        out_options_ptr->arguments.resource_id     = value;
    }

    /* The paging options (limit and offset) are common to all query tools; copy them into the arguments. */
    out_options_ptr->arguments.limit  = out_options_ptr->common.limit;
    out_options_ptr->arguments.offset = out_options_ptr->common.offset;

    result = true;

end:
    return result;
}

void Analysis::APIDumpDependencyQuery::print_usage()
{
    fprintf(stdout,
            "Usage:\n"
            "\n"
            "APIDumpDependencyQuery drive:\\path\\to\\file.deps <command> [id] [options]\n"
            "\n"
            "Prints what APIDumpDependencyExtractor found in a workload, as JSON: what each frame does with what the frames before it left in the\n"
            "resources.\n"
            "\n"
            "Commands:\n"
            "  info                 What the file is of, and how many resources are of each kind of dependency\n"
            "  resources            Every resource with its totals (--class to choose a kind of dependency)\n"
            "  resource <id>        One resource: its totals, the runs of frames which use it alike, and the versions of its content\n"
            "  frame <index>        One frame: the calls it spans, and what it does with each resource it touches\n"
            "  carries <index>      What a frame uses of what the frames before it left, and which frame left it\n"
            "  per-eye              The resources which each eye is to have a copy of, as a recipe names them\n"
            "  slice <index>        The frames whose content a replay that goes to the frame has to make, for it and the frames after it to come out as they do\n"
            "                       when every frame is made; the others are dead, and can be left out\n"
            "\n"
            "Options:\n"
            "  --class C            Only the resources of a class: carried, carried_possibly, frame_local, static, unused\n"
            "  --compact            Print JSON on a single line\n"
            "  --limit N            Items in a list (default 100; 0: all)\n"
            "  --no-verify          Do not check that the workload is the one the file is of\n"
            "  --offset N           Items to skip at the start of a list\n"
            "  --workload PATH      Where the workload is, if not where the file says (it is checked against the file)\n"
            "\n"
            "Frames and resources are numbered as in the other files of the analysis (a resource from 1, a frame from 0).\n"
            "Exit codes: 0 done, 1 bad arguments, 2 file cannot be read, 3 nothing there by that id, 4 the workload is not the one the file is of.\n"
            "A workload which cannot be found is not an error: the questions do not need it, and the check is skipped.\n");
}

int Analysis::APIDumpDependencyQuery::run(int   in_argc,
                                          char* in_argv[])
{
    Analysis::APIDumpDependencies::DependencyAnalysis analysis;
    std::string                                       error;
    Analysis::APIDumpAnalysis::QueryExitCode          exit_code = Analysis::APIDumpAnalysis::QueryExitCode::SUCCESS;
    Options                                           options;

    if (parse_arguments(in_argc,
                        in_argv,
                       &options,
                       &error) == false)
    {
        fprintf(stderr,
                "Error: %s\n\n",
                error.c_str() );

        print_usage();

        exit_code = Analysis::APIDumpAnalysis::QueryExitCode::BAD_ARGUMENTS;

        goto end;
    }

    {
        const auto command_func = [&](Utils::Json::JsonWriter* in_writer_ptr,
                                      std::string*             out_error_ptr)
        {
            return Analysis::APIDumpDependencies::QueryCommands::run(options.command,
                                                                     analysis,
                                                                     options.arguments,
                                                                     in_writer_ptr,
                                                                     out_error_ptr);
        };

        const auto read_func = [&](const std::string& in_filename,
                                   std::string*       out_error_ptr)
        {
            return Analysis::APIDumpDependencies::DepsFile::read(in_filename,
                                                                &analysis,
                                                                 out_error_ptr);
        };

        const auto reference_func = [&]() -> const Analysis::APIDumpAnalysis::WorkloadReference&
        {
            return analysis.workload_reference;
        };

        exit_code = Analysis::APIDumpAnalysis::QueryProgramRunner::run(options.common,
                                                                       options.deps_filename,
                                                                       "file",
                                                                       read_func,
                                                                       reference_func,
                                                                       command_func);
    }

end:
    return static_cast<int>(exit_code);
}

int main(int   in_argc,
         char* in_argv[])
{
    return Analysis::APIDumpDependencyQuery::run(in_argc,
                                                 in_argv);
}
