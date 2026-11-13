/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_analysis/passes_file.h"
#include "analysis/api_dump_analysis/query_command_line.h"
#include "analysis/api_dump_analysis/query_commands.h"
#include "analysis/api_dump_analysis/query_tool.h"
#include "query.h"
#include "utils/command_line/number_parser.h"
#include <stdio.h>
#include <stdlib.h>

bool Analysis::APIDumpAnalysisQuery::parse_arguments(const int&   in_argc,
                                                     char*        in_argv[],
                                                     Options*     out_options_ptr,
                                                     std::string* out_error_ptr)
{
    typedef Analysis::APIDumpAnalysis::QueryCommandLine QueryCommandLine;

    Analysis::APIDumpAnalysis::QueryPositionalRules positional_rules;
    Analysis::APIDumpAnalysis::QueryPositionals     positionals;
    bool                                            result = false;

    /* What the tool has besides the options that every query tool has: --shape, which takes a number, and --class and --which. */
    const auto set_shape_id_func = [&](const uint64_t& in_value)
    {
        out_options_ptr->arguments.has_shape_id = true;
        out_options_ptr->arguments.shape_id     = in_value;
    };

    const std::vector<Analysis::APIDumpAnalysis::QueryNumberOption> number_option_vec =
    {
        {"--shape", set_shape_id_func}
    };

    const QueryCommandLine::ProgramOptionFunc program_option_func = [&](const std::string& in_argument,
                                                                        const bool&        in_has_next,
                                                                        const std::string& in_next_argument,
                                                                        bool*              out_is_known_ptr,
                                                                        bool*              out_uses_next_ptr,
                                                                        std::string*       out_error_ptr_inner)
    {
        bool is_set = true;

        *out_is_known_ptr  = (in_argument == "--class" ||
                              in_argument == "--which");
        *out_uses_next_ptr = *out_is_known_ptr;

        if (*out_is_known_ptr == false)
        {
            /* Not one of ours. */
        }
        else
        if (in_has_next == false)
        {
            *out_error_ptr_inner = (in_argument == "--class") ? "--class needs frame_local, loop_carried, static, unread or unused"
                                                              : "--which needs first, lightest or heaviest";
            is_set               = false;
        }
        else
        if (in_argument == "--class")
        {
            if (Analysis::APIDumpAnalysis::QueryCommands::parse_resource_access_class(in_next_argument,
                                                                                     &out_options_ptr->arguments.access_class) == false)
            {
                *out_error_ptr_inner = "--class needs frame_local, loop_carried, static, unread or unused, not " + in_next_argument;
                is_set               = false;
            }
            else
            {
                out_options_ptr->arguments.has_access_class = true;
            }
        }
        else
        if (in_next_argument == "first")
        {
            out_options_ptr->arguments.which = Analysis::APIDumpAnalysis::RepresentativeChoice::FIRST;
        }
        else
        if (in_next_argument == "heaviest")
        {
            out_options_ptr->arguments.which = Analysis::APIDumpAnalysis::RepresentativeChoice::HEAVIEST;
        }
        else
        if (in_next_argument == "lightest")
        {
            out_options_ptr->arguments.which = Analysis::APIDumpAnalysis::RepresentativeChoice::LIGHTEST;
        }
        else
        {
            *out_error_ptr_inner = "--which needs first, lightest or heaviest, not " + in_next_argument;
            is_set               = false;
        }

        return is_set;
    };

    /* The file, the command, and then the id of a pass or the index of a frame. */
    positional_rules.max_n_numbers     = 1;
    positional_rules.missing_text      = "A .passes file and a command are needed";
    positional_rules.number_error_text = "An id must be a number, not ";

    if (QueryCommandLine::parse_file_command_and_numbers(in_argc,
                                                         in_argv,
                                                         number_option_vec,
                                                         program_option_func,
                                                         positional_rules,
                                                        &out_options_ptr->common,
                                                        &positionals,
                                                         out_error_ptr) == false)
    {
        goto end;
    }

    out_options_ptr->command         = positionals.command;
    out_options_ptr->passes_filename = positionals.filename;

    if (positionals.number_vec.empty() == false)
    {
        const uint64_t value = positionals.number_vec.at(0);

        out_options_ptr->arguments.frame_index     = value;
        out_options_ptr->arguments.has_frame_index = (out_options_ptr->command == "frame");
        out_options_ptr->arguments.has_pass_id     = (out_options_ptr->command == "pass");
        out_options_ptr->arguments.pass_id         = value;
    }

    /* The paging of the lists is read with the other options that every query tool has. */
    out_options_ptr->arguments.limit  = out_options_ptr->common.limit;
    out_options_ptr->arguments.offset = out_options_ptr->common.offset;

    result = true;

end:
    return result;
}

void Analysis::APIDumpAnalysisQuery::print_usage()
{
    fprintf(stdout,
            "Usage:\n"
            "\n"
            "APIDumpAnalysisQuery drive:\\path\\to\\file.passes <command> [id] [options]\n"
            "\n"
            "Prints what APIDumpAnalysisExtractor found in a workload, as JSON.\n"
            "\n"
            "Commands:\n"
            "  depth                How deep the draws of each pass are: the near, median and far depth, and how many vertices\n"
            "  info                 What the analysis is of, how the workload was read, how much was found\n"
            "  timeline             The runs of frames of one kind, in order of time\n"
            "  pipelines            The kinds of frames, and the kinds of passes each is made of\n"
            "  resources            What the frames did with each resource, and which can be played again\n"
            "  overlays             The passes whose camera never changes (maybe 2D overlays), and their draws, and the draws drawn with an orthographic projection, as an \"unaffected\" list for a recipe\n"
            "  pass <id>            One kind of pass: its targets, draws and camera constants\n"
            "  frame <index>        One frame (its calls, and its passes if it is a representative frame)\n"
            "  frame --shape <id>   A representative frame of one kind of frame\n"
            "\n"
            "Options:\n"
            "  --class C            Only the resources of a class: frame_local, loop_carried, static, unread, unused\n"
            "  --compact            Print JSON on a single line\n"
            "  --limit N            Items in a list (default 100; 0: all)\n"
            "  --no-verify          Do not check that the workload is the one the analysis is of\n"
            "  --offset N           Items to skip at the start of a list\n"
            "  --shape N            The kind of frame, for frame\n"
            "  --which W            first, lightest or heaviest representative frame (default first)\n"
            "  --workload PATH      Where the workload is, if not where the analysis says (it is checked against the analysis)\n"
            "\n"
            "Times are in seconds since the first call; keys are hex strings.\n"
            "Exit codes: 0 done, 1 bad arguments, 2 file cannot be read, 3 nothing there by that id, 4 the workload is not the one\n"
            "the analysis is of. A workload which cannot be found is not an error: the analysis does not need it, and the check is skipped.\n");
}

int Analysis::APIDumpAnalysisQuery::run(int   in_argc,
                                        char* in_argv[])
{
    Analysis::APIDumpAnalysis::AnalysisResult analysis;
    std::string                               error;
    Analysis::APIDumpAnalysis::QueryExitCode  exit_code = Analysis::APIDumpAnalysis::QueryExitCode::SUCCESS;
    Options                                   options;

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
            return Analysis::APIDumpAnalysis::QueryCommands::run(options.command,
                                                                 analysis,
                                                                 options.arguments,
                                                                 in_writer_ptr,
                                                                 out_error_ptr);
        };

        const auto read_func = [&](const std::string& in_filename,
                                   std::string*       out_error_ptr)
        {
            return Analysis::APIDumpAnalysis::PassesFile::read(in_filename,
                                                              &analysis,
                                                               out_error_ptr);
        };

        const auto reference_func = [&]() -> const Analysis::APIDumpAnalysis::WorkloadReference&
        {
            return analysis.workload_reference;
        };

        exit_code = Analysis::APIDumpAnalysis::QueryTool::run(options.common,
                                                              options.passes_filename,
                                                              "analysis",
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
    return Analysis::APIDumpAnalysisQuery::run(in_argc,
                                               in_argv);
}
