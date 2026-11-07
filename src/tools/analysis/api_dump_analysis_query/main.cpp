/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_analysis/passes_file.h"
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
    int  n_positionals = 0;
    bool result        = false;

    for (int n_argument = 1;
             n_argument < in_argc;
           ++n_argument)
    {
        const std::string argument      = in_argv[n_argument];
        const bool        has_next      = (n_argument + 1 < in_argc);
        const std::string next_argument = (has_next) ? in_argv[n_argument + 1]
                                                     : "";
        const bool        takes_number  = (argument == "--limit"  ||
                                           argument == "--offset" ||
                                           argument == "--shape");

        if (argument == "--compact")
        {
            out_options_ptr->is_compact = true;
        }
        else
        if (argument == "--no-verify")
        {
            out_options_ptr->is_verify_off = true;
        }
        else
        if (argument == "--workload")
        {
            if (has_next == false)
            {
                *out_error_ptr = "--workload needs the path of the workload";

                goto end;
            }

            out_options_ptr->workload_filename = next_argument;

            ++n_argument;
        }
        else
        if (takes_number)
        {
            uint64_t value = 0;

            if (has_next == false)
            {
                *out_error_ptr = argument + " needs a number";

                goto end;
            }

            if (Utils::CommandLine::NumberParser::parse_unsigned(next_argument,
                                                                 UINT64_MAX,
                                                                 &value) == false)
            {
                *out_error_ptr = argument + " needs a number, not " + next_argument;

                goto end;
            }

            if (argument == "--limit")
            {
                out_options_ptr->arguments.limit = value;
            }
            else
            if (argument == "--offset")
            {
                out_options_ptr->arguments.offset = value;
            }
            else
            {
                out_options_ptr->arguments.has_shape_id = true;
                out_options_ptr->arguments.shape_id     = value;
            }

            ++n_argument;
        }
        else
        if (argument == "--class")
        {
            if (has_next == false)
            {
                *out_error_ptr = "--class needs frame_local, loop_carried, static, unread or unused";

                goto end;
            }

            if (Analysis::APIDumpAnalysis::QueryCommands::parse_resource_access_class(next_argument,
                                                                                     &out_options_ptr->arguments.access_class) == false)
            {
                *out_error_ptr = "--class needs frame_local, loop_carried, static, unread or unused, not " + next_argument;

                goto end;
            }

            out_options_ptr->arguments.has_access_class = true;

            ++n_argument;
        }
        else
        if (argument == "--which")
        {
            if (has_next == false)
            {
                *out_error_ptr = "--which needs first, lightest or heaviest";

                goto end;
            }

            if (next_argument == "first")
            {
                out_options_ptr->arguments.which = Analysis::APIDumpAnalysis::RepresentativeChoice::FIRST;
            }
            else
            if (next_argument == "heaviest")
            {
                out_options_ptr->arguments.which = Analysis::APIDumpAnalysis::RepresentativeChoice::HEAVIEST;
            }
            else
            if (next_argument == "lightest")
            {
                out_options_ptr->arguments.which = Analysis::APIDumpAnalysis::RepresentativeChoice::LIGHTEST;
            }
            else
            {
                *out_error_ptr = "--which needs first, lightest or heaviest, not " + next_argument;

                goto end;
            }

            ++n_argument;
        }
        else
        if (argument.empty() == false &&
            argument.at(0)   == '-')
        {
            *out_error_ptr = "Unknown option " + argument;

            goto end;
        }
        else
        if (n_positionals == 0)
        {
            out_options_ptr->passes_filename = argument;

            ++n_positionals;
        }
        else
        if (n_positionals == 1)
        {
            out_options_ptr->command = argument;

            ++n_positionals;
        }
        else
        if (n_positionals == 2)
        {
            /* The id of a pass, or the index of a frame. */
            uint64_t value = 0;

            if (Utils::CommandLine::NumberParser::parse_unsigned(argument,
                                                                 UINT64_MAX,
                                                                 &value) == false)
            {
                *out_error_ptr = "An id must be a number, not " + argument;

                goto end;
            }

            out_options_ptr->arguments.frame_index     = value;
            out_options_ptr->arguments.has_frame_index = (out_options_ptr->command == "frame");
            out_options_ptr->arguments.has_pass_id     = (out_options_ptr->command == "pass");
            out_options_ptr->arguments.pass_id         = value;

            ++n_positionals;
        }
        else
        {
            *out_error_ptr = "Too many arguments";

            goto end;
        }
    }

    if (n_positionals < 2)
    {
        *out_error_ptr = "A .passes file and a command are needed";

        goto end;
    }

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
            "  overlays             The passes whose camera never changes (maybe 2D overlays), and their draws, as an \"unaffected\" list for a recipe\n"
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

    if (options.is_verify_off == false)
    {
        const std::string workload_filename = (options.workload_filename.empty() ) ? analysis.workload_reference.filename
                                                                                   : options.workload_filename;

        const auto match = Analysis::APIDumpAnalysis::WorkloadIdentity::verify(workload_filename,
                                                                               analysis.workload_reference,
                                                                              &error);

        if (match == Analysis::APIDumpAnalysis::WorkloadMatch::MISMATCH)
        {
            fprintf(stderr,
                    "Error: %s is not the workload this analysis is of: %s\n",
                    workload_filename.c_str(),
                    error.c_str() );

            exit_code = ExitCode::WORKLOAD_MISMATCH;

            goto end;
        }

        if (match == Analysis::APIDumpAnalysis::WorkloadMatch::UNREADABLE)
        {
            fprintf(stderr,
                    "Warning: the workload could not be checked against the analysis: %s\n",
                    error.c_str() );
        }
    }

    {
        Utils::Json::JsonWriter writer(options.is_compact == false);
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
