/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_analysis/extractor_command_line.h"
#include "utils/command_line/argument_walker.h"
#include "utils/command_line/number_parser.h"
#include <algorithm>
#include <thread>

std::string Analysis::APIDumpAnalysis::ExtractorCommandLine::get_default_output_filename(const std::string& in_input_filename,
                                                                                         const std::string& in_extension)
{
    const size_t extension_position = in_input_filename.find_last_of(".\\/");
    std::string  result             = in_input_filename;

    if (extension_position                       != std::string::npos &&
        in_input_filename.at(extension_position) == '.')
    {
        result.resize(extension_position);
    }

    return result + in_extension;
}

bool Analysis::APIDumpAnalysis::ExtractorCommandLine::parse(const int&               in_argc,
                                                            char*                    in_argv[],
                                                            const ExtractorProfile&  in_profile,
                                                            const ProgramOptionFunc& in_program_option_func,
                                                            ExtractorOptions*        out_options_ptr,
                                                            std::string*             out_error_ptr)
{
    /* More threads than this would only wait for the memory: reading is a matter of how much is moved. */
    constexpr uint32_t DEFAULT_MAX_N_THREADS = 8;
    constexpr uint64_t MAX_N_THREADS         = 64;

    bool result             = false;
    bool is_n_threads_given = false;

    out_options_ptr->skip_data_chunks_above_n_bytes = in_profile.default_skip_data_chunks_above_n_bytes;

    const auto parse_argument_func = [&](const Utils::CommandLine::ArgumentCursor& in_cursor,
                                         bool*                                     out_uses_next_ptr,
                                         std::string*                              out_error_ptr_inner)
    {
        const std::string& argument      = in_cursor.get_argument();
        const bool         has_next      = in_cursor.has_next();
        const std::string& next_argument = in_cursor.get_next_argument();
        bool               is_known      = false;
        bool               uses_next     = false;

        if (in_cursor.ask(in_program_option_func,
                         &is_known,
                         &uses_next,
                          out_error_ptr_inner) == false)
        {
            return false;
        }

        if (is_known)
        {
            *out_uses_next_ptr = uses_next;
        }
        else
        if (argument == "-o")
        {
            if (has_next == false)
            {
                *out_error_ptr_inner = "-o needs a file name";

                return false;
            }

            out_options_ptr->output_filename = next_argument;

            *out_uses_next_ptr = true;
        }
        else
        if (argument == "--max-frames")
        {
            if (has_next == false)
            {
                *out_error_ptr_inner = "--max-frames needs a number";

                return false;
            }

            if (Utils::CommandLine::NumberParser::parse_unsigned(next_argument,
                                                                 UINT64_MAX,
                                                                 &out_options_ptr->max_frames) == false)
            {
                *out_error_ptr_inner = "--max-frames needs a number, not " + next_argument;

                return false;
            }

            *out_uses_next_ptr = true;
        }
        else
        if (argument == "--progress-interval")
        {
            if (has_next == false)
            {
                *out_error_ptr_inner = "--progress-interval needs a number of seconds";

                return false;
            }

            if (Utils::CommandLine::NumberParser::parse_double(next_argument,
                                                               &out_options_ptr->progress_interval_seconds) == false)
            {
                *out_error_ptr_inner = "--progress-interval needs a number of seconds, not " + next_argument;

                return false;
            }

            *out_uses_next_ptr = true;
        }
        else
        if (argument == "--skip-chunks-above")
        {
            uint64_t n_bytes = 0;

            if (has_next == false)
            {
                *out_error_ptr_inner = "--skip-chunks-above needs a number of bytes";

                return false;
            }

            if (Utils::CommandLine::NumberParser::parse_unsigned(next_argument,
                                                                 UINT32_MAX,
                                                                 &n_bytes) == false)
            {
                *out_error_ptr_inner = "--skip-chunks-above needs a number of bytes, not " + next_argument;

                return false;
            }

            out_options_ptr->skip_data_chunks_above_n_bytes = static_cast<uint32_t>(n_bytes);

            *out_uses_next_ptr = true;
        }
        else
        if (argument == "--stats")
        {
            out_options_ptr->print_stats = true;
        }
        else
        if (argument == "--threads")
        {
            uint64_t n_threads = 0;

            if (has_next == false)
            {
                *out_error_ptr_inner = "--threads needs a number";

                return false;
            }

            if (Utils::CommandLine::NumberParser::parse_unsigned(next_argument,
                                                                 MAX_N_THREADS,
                                                                 &n_threads) == false)
            {
                *out_error_ptr_inner = "--threads needs a number up to " + std::to_string(MAX_N_THREADS) + ", not " + next_argument;

                return false;
            }

            out_options_ptr->n_threads = static_cast<uint32_t>(n_threads);
            is_n_threads_given         = true;

            *out_uses_next_ptr = true;
        }
        else
        if (argument.empty() == false &&
            argument.at(0)   == '-')
        {
            *out_error_ptr_inner = "Unknown option " + argument;

            return false;
        }
        else
        if (out_options_ptr->input_filename.empty() )
        {
            out_options_ptr->input_filename = argument;
        }
        else
        {
            *out_error_ptr_inner = "More than one workload given";

            return false;
        }

        return true;
    };

    if (Utils::CommandLine::ArgumentWalker::walk(in_argc,
                                                 in_argv,
                                                 parse_argument_func,
                                                 out_error_ptr) == false)
    {
        goto end;
    }

    if (out_options_ptr->input_filename.empty() )
    {
        *out_error_ptr = "No workload given";

        goto end;
    }

    if (out_options_ptr->output_filename.empty() )
    {
        out_options_ptr->output_filename = get_default_output_filename(out_options_ptr->input_filename,
                                                                       in_profile.output_extension);
    }

    if (is_n_threads_given == false)
    {
        out_options_ptr->n_threads = std::min<uint32_t>(DEFAULT_MAX_N_THREADS,
                                                        std::thread::hardware_concurrency() );
    }

    result = true;

end:
    return result;
}
