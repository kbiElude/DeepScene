/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_analysis/query_command_line.h"
#include "analysis/api_dump_analysis/workload_identity.h"
#include "analysis/api_dump_textures/textures_file.h"
#include "query.h"
#include "utils/command_line/number_parser.h"
#include "utils/json/json_writer.h"
#include <stdio.h>
#include <stdlib.h>
#include <vector>

bool Analysis::APIDumpTextureQuery::parse_arguments(const int&   in_argc,
                                                    char*        in_argv[],
                                                    Options*     out_options_ptr,
                                                    std::string* out_error_ptr)
{
    typedef Analysis::APIDumpAnalysis::QueryCommandLine QueryCommandLine;

    std::vector<uint64_t> number_vec;
    bool                  result = false;

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
    const QueryCommandLine::PositionalFunc positional_func = [&](const std::string& in_argument,
                                                                 const size_t&      in_n_positional,
                                                                 std::string*       out_error_ptr_inner)
    {
        uint64_t value  = 0;
        bool     is_set = true;

        if (in_n_positional == 0)
        {
            out_options_ptr->textures_filename = in_argument;
        }
        else
        if (in_n_positional == 1)
        {
            out_options_ptr->command = in_argument;
        }
        else
        if (Utils::CommandLine::NumberParser::parse_unsigned(in_argument,
                                                             UINT64_MAX,
                                                            &value) == false)
        {
            *out_error_ptr_inner = "A frame, a resource or a level must be a number, not " + in_argument;
            is_set               = false;
        }
        else
        {
            number_vec.push_back(value);
        }

        return is_set;
    };

    if (QueryCommandLine::parse(in_argc,
                                in_argv,
                                number_option_vec,
                                QueryCommandLine::ProgramOptionFunc(),
                                positional_func,
                                &out_options_ptr->common,
                                out_error_ptr) == false)
    {
        goto end;
    }

    if (out_options_ptr->textures_filename.empty() ||
        out_options_ptr->command.empty()           )
    {
        *out_error_ptr = "A .textures file and a command are needed";

        goto end;
    }

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
    std::string                                 error;
    ExitCode                                    exit_code = ExitCode::SUCCESS;
    Options                                     options;
    Analysis::APIDumpTextures::TextureTimeline  timeline;

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

    if (Analysis::APIDumpTextures::TexturesFile::read(options.textures_filename,
                                                     &timeline,
                                                     &error) == false)
    {
        fprintf(stderr,
                "Error: %s\n",
                error.c_str() );

        exit_code = ExitCode::CANNOT_READ_TEXTURES;

        goto end;
    }

    if (options.common.is_verify_off == false)
    {
        const auto match = Analysis::APIDumpAnalysis::WorkloadIdentity::check_for_query(options.common.workload_filename,
                                                                                         timeline.workload_reference,
                                                                                         "file");

        if (match == Analysis::APIDumpAnalysis::WorkloadMatch::MISMATCH)
        {
            exit_code = ExitCode::WORKLOAD_MISMATCH;

            goto end;
        }
    }

    {
        Utils::Json::JsonWriter writer(options.common.is_compact == false);

        const auto status = Analysis::APIDumpTextures::QueryCommands::run(options.command,
                                                                          timeline,
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
    return Analysis::APIDumpTextureQuery::run(in_argc,
                                              in_argv);
}
