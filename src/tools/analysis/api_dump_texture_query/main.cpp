/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_analysis/query_command_line.h"
#include "analysis/api_dump_analysis/query_tool.h"
#include "analysis/api_dump_textures/textures_file.h"
#include "query.h"
#include "utils/command_line/number_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <vector>

bool Analysis::APIDumpTextureQuery::parse_arguments(const int&   in_argc,
                                                    char*        in_argv[],
                                                    Options*     out_options_ptr,
                                                    std::string* out_error_ptr)
{
    typedef Analysis::APIDumpAnalysis::QueryCommandLine QueryCommandLine;

    Analysis::APIDumpAnalysis::QueryPositionalRules positional_rules;
    Analysis::APIDumpAnalysis::QueryPositionals     positionals;
    bool                                            result = false;

    /* What the tool has besides the options that every query tool has: --call, which takes a number. */
    const auto set_call_limit_func = [&](const uint64_t& in_value)
    {
        out_options_ptr->arguments.call_limit     = in_value;
        out_options_ptr->arguments.has_call_limit = true;
    };

    const std::vector<Analysis::APIDumpAnalysis::QueryNumberOption> number_option_vec =
    {
        {"--call", set_call_limit_func}
    };

    /* The file, the command, and then the numbers: the frame, the resource and the level of at, or the resource of resource. */
    positional_rules.max_n_numbers     = SIZE_MAX;
    positional_rules.missing_text      = "A .textures file and a command are needed";
    positional_rules.number_error_text = "A frame, a resource or a level must be a number, not ";

    if (QueryCommandLine::parse_file_command_and_numbers(in_argc,
                                                         in_argv,
                                                         number_option_vec,
                                                         QueryCommandLine::ProgramOptionFunc(),
                                                         positional_rules,
                                                        &out_options_ptr->common,
                                                        &positionals,
                                                         out_error_ptr) == false)
    {
        goto end;
    }

    out_options_ptr->command           = positionals.command;
    out_options_ptr->textures_filename = positionals.filename;

    const std::vector<uint64_t>& number_vec = positionals.number_vec;

    if (out_options_ptr->command == "at")
    {
        if (number_vec.size() != 3)
        {
            *out_error_ptr = "at needs a frame, a resource and a level: at <frame> <resource> <level>";

            goto end;
        }

        out_options_ptr->arguments.frame_index     = number_vec.at(0);
        out_options_ptr->arguments.has_frame_index = true;
        out_options_ptr->arguments.has_mip_level   = true;
        out_options_ptr->arguments.has_resource_id = true;
        out_options_ptr->arguments.mip_level       = number_vec.at(2);
        out_options_ptr->arguments.resource_id     = number_vec.at(1);
    }
    else
    if (out_options_ptr->command == "resource")
    {
        if (number_vec.size() != 1)
        {
            *out_error_ptr = "resource needs the id of a resource: resource <id>";

            goto end;
        }

        out_options_ptr->arguments.has_resource_id = true;
        out_options_ptr->arguments.resource_id     = number_vec.at(0);
    }
    else
    if (number_vec.empty() == false)
    {
        *out_error_ptr = out_options_ptr->command + " takes no numbers";

        goto end;
    }

    /* The paging of the lists is read with the other options that every query tool has. */
    out_options_ptr->arguments.limit  = out_options_ptr->common.limit;
    out_options_ptr->arguments.offset = out_options_ptr->common.offset;

    result = true;

end:
    return result;
}

void Analysis::APIDumpTextureQuery::print_usage()
{
    fprintf(stdout,
            "Usage:\n"
            "\n"
            "APIDumpTextureQuery drive:\\path\\to\\file.textures <command> [numbers] [options]\n"
            "\n"
            "Prints what APIDumpTextureExtractor found in a workload, as JSON: what happens to the textures, and where the bytes of what they hold are.\n"
            "\n"
            "Commands:\n"
            "  info                           What the file is of, the events of each kind, and where the contents are\n"
            "  resources                      Every texture that has events, with the events of each kind and the levels it has\n"
            "  resource <id>                  One texture: its levels (with their size and format), and its events in the order of the calls\n"
            "  at <frame> <resource> <level>  What a level holds at the end of a frame: the event that made each part of it, where the bytes are (a chunk of the\n"
            "                                 workload, or the file of blobs; a part that rendering made has none), and the last snapshot point of the level\n"
            "\n"
            "Options:\n"
            "  --call N             at: count the events only up to call N of the frame (default: all of the frame)\n"
            "  --compact            Print JSON on a single line\n"
            "  --limit N            Items in a list (default 100; 0: all)\n"
            "  --no-verify          Do not check that the workload is the one the file is of\n"
            "  --offset N           Items to skip at the start of a list\n"
            "  --workload PATH      Where the workload is, if not where the file says (it is checked against the file)\n"
            "\n"
            "Frames are numbered from 0 and resources from 1, as in the other files of the analysis; the levels from 0.\n"
            "Exit codes: 0 done, 1 bad arguments, 2 file cannot be read, 3 nothing there by that id, 4 the workload is not the one the file is of.\n"
            "A workload which cannot be found is not an error: the questions do not need it, and the check is skipped.\n");
}

int Analysis::APIDumpTextureQuery::run(int   in_argc,
                                       char* in_argv[])
{
    std::string                                  error;
    Analysis::APIDumpAnalysis::QueryExitCode     exit_code = Analysis::APIDumpAnalysis::QueryExitCode::SUCCESS;
    Options                                      options;
    Analysis::APIDumpTextures::TextureTimeline   timeline;

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
            return Analysis::APIDumpTextures::QueryCommands::run(options.command,
                                                                 timeline,
                                                                 options.arguments,
                                                                 in_writer_ptr,
                                                                 out_error_ptr);
        };

        const auto read_func = [&](const std::string& in_filename,
                                   std::string*       out_error_ptr)
        {
            return Analysis::APIDumpTextures::TexturesFile::read(in_filename,
                                                                &timeline,
                                                                 out_error_ptr);
        };

        const auto reference_func = [&]() -> const Analysis::APIDumpAnalysis::WorkloadReference&
        {
            return timeline.workload_reference;
        };

        exit_code = Analysis::APIDumpAnalysis::QueryTool::run(options.common,
                                                              options.textures_filename,
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
    return Analysis::APIDumpTextureQuery::run(in_argc,
                                              in_argv);
}
