/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_analysis/passes_file.h"
#include "analysis/api_dump_analysis/query_command_line.h"
#include "analysis/api_dump_analysis/query_commands.h"
#include "analysis/api_dump_analysis/workload_identity.h"
#include "query.h"
#include "utils/command_line/number_parser.h"
#include "utils/json/json_writer.h"
#include <stdio.h>
#include <stdlib.h>

bool Analysis::APIDumpAnalysisQuery::parse_arguments(const int&   in_argc,
                                                     char*        in_argv[],
                                                     Options*     out_options_ptr,
                                                     std::string* out_error_ptr)
{
    typedef Analysis::APIDumpAnalysis::QueryCommandLine QueryCommandLine;

    bool result = false;

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
    const QueryCommandLine::PositionalFunc positional_func = [&](const std::string& in_argument,
                                                                 const size_t&      in_n_positional,
                                                                 std::string*       out_error_ptr_inner)
    {
        uint64_t value  = 0;
        bool     is_set = true;

        if (in_n_positional == 0)
        {
            out_options_ptr->passes_filename = in_argument;
        }
        else
        if (in_n_positional == 1)
        {
            out_options_ptr->command = in_argument;
        }
        else
        if (in_n_positional > 2)
        {
            *out_error_ptr_inner = "Too many arguments";
            is_set               = false;
        }
        else
        if (Utils::CommandLine::NumberParser::parse_unsigned(in_argument,
                                                             UINT64_MAX,
                                                            &value) == false)
        {
            *out_error_ptr_inner = "An id must be a number, not " + in_argument;
            is_set               = false;
        }
        else
        {
            out_options_ptr->arguments.frame_index     = value;
            out_options_ptr->arguments.has_frame_index = (out_options_ptr->command == "frame");
            out_options_ptr->arguments.has_pass_id     = (out_options_ptr->command == "pass");
            out_options_ptr->arguments.pass_id         = value;
        }

        return is_set;
    };

    if (QueryCommandLine::parse(in_argc,
                                in_argv,
                                number_option_vec,
                                program_option_func,
                                positional_func,
                                &out_options_ptr->common,
                                out_error_ptr) == false)
    {
        goto end;
    }

    if (out_options_ptr->passes_filename.empty() ||
        out_options_ptr->command.empty()         )
    {
        *out_error_ptr = "A .passes file and a command are needed";

        goto end;
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
    ExitCode                                  exit_code = ExitCode::SUCCESS;
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

        exit_code = ExitCode::BAD_ARGUMENTS;

        goto end;
    }

    if (Analysis::APIDumpAnalysis::PassesFile::read(options.passes_filename,
                                                   &analysis,
                                                   &error) == false)
    {
        fprintf(stderr,
                "Error: %s\n",
                error.c_str() );

        exit_code = ExitCode::CANNOT_READ_PASSES;

        goto end;
    }

    if (options.common.is_verify_off == false)
    {
        const auto match = Analysis::APIDumpAnalysis::WorkloadIdentity::check_for_query(options.common.workload_filename,
                                                                                         analysis.workload_reference,
                                                                                         "analysis");

        if (match == Analysis::APIDumpAnalysis::WorkloadMatch::MISMATCH)
        {
            exit_code = ExitCode::WORKLOAD_MISMATCH;

            goto end;
        }
    }

    {
        Utils::Json::JsonWriter writer(options.common.is_compact == false);
        const auto                  status = Analysis::APIDumpAnalysis::QueryCommands::run(options.command,
                                                                                           analysis,
                                                                                           options.arguments,
                                                                                          &writer,
                                                                                          &error);

        if (status == Analysis::APIDumpAnalysis::QueryStatus::OK)
        {
            fprintf(stdout,
                    "%s\n",
                    writer.get_text().c_str() );
        }
        else
        {
            fprintf(stderr,
                    "Error: %s\n",
                    error.c_str() );

            exit_code = (status == Analysis::APIDumpAnalysis::QueryStatus::NOT_FOUND) ? ExitCode::NOT_FOUND
                                                                                      : ExitCode::BAD_ARGUMENTS;
        }
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
