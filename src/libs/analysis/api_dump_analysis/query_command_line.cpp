/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_analysis/query_command_line.h"
#include "utils/command_line/argument_walker.h"
#include "utils/command_line/number_parser.h"

namespace
{
    /* Handles the argument if it is one of the options that every query program has. */
    bool parse_common_option(const std::string&                       in_argument,
                             const bool&                              in_has_next,
                             const std::string&                       in_next_argument,
                             Analysis::APIDumpAnalysis::QueryOptions* out_options_ptr,
                             bool*                                    out_is_known_ptr,
                             bool*                                    out_uses_next_ptr,
                             std::string*                             out_error_ptr)
    {
        bool result = true;

        *out_is_known_ptr  = true;
        *out_uses_next_ptr = false;

        if (in_argument == "--compact")
        {
            out_options_ptr->is_compact = true;
        }
        else
        if (in_argument == "--no-verify")
        {
            out_options_ptr->is_verify_off = true;
        }
        else
        if (in_argument == "--workload")
        {
            if (in_has_next == false)
            {
                *out_error_ptr = "--workload needs the path of the workload";
                result         = false;

                goto end;
            }

            out_options_ptr->workload_filename = in_next_argument;
            *out_uses_next_ptr                 = true;
        }
        else
        if (in_argument == "--limit" ||
            in_argument == "--offset")
        {
            uint64_t value = 0;

            if (Utils::CommandLine::NumberParser::parse_option_unsigned(in_argument,
                                                                        in_has_next,
                                                                        in_next_argument,
                                                                        UINT64_MAX,
                                                                       &value,
                                                                        out_error_ptr) == false)
            {
                result = false;

                goto end;
            }

            if (in_argument == "--limit")
            {
                out_options_ptr->limit = value;
            }
            else
            {
                out_options_ptr->offset = value;
            }

            *out_uses_next_ptr = true;
        }
        else
        {
            *out_is_known_ptr = false;
        }

    end:
        return result;
    }
}

bool Analysis::APIDumpAnalysis::QueryCommandLine::parse(const int&                            in_argc,
                                                        char*                                 in_argv[],
                                                        const std::vector<QueryNumberOption>& in_number_option_vec,
                                                        const ProgramOptionFunc&              in_program_option_func,
                                                        const PositionalFunc&                 in_positional_func,
                                                        QueryOptions*                         out_options_ptr,
                                                        std::string*                          out_error_ptr)
{
    size_t n_positionals = 0;

    /* What is done with each argument: an option that every query program has, an option that takes a number, an option of the program, or what is not an option. */
    const auto parse_argument_func = [&](const Utils::CommandLine::ArgumentCursor& in_cursor,
                                         bool*                                     out_uses_next_ptr,
                                         std::string*                              out_error_ptr_inner)
    {
        const std::string& argument = in_cursor.get_argument();

        bool is_known = false;
        bool is_valid = true;

        is_valid = parse_common_option(argument,
                                       in_cursor.has_next(),
                                       in_cursor.get_next_argument(),
                                       out_options_ptr,
                                      &is_known,
                                       out_uses_next_ptr,
                                       out_error_ptr_inner);

        for (const auto& number_option : in_number_option_vec)
        {
            uint64_t value = 0;

            if (is_valid == false          ||
                is_known                   ||
                number_option.name != argument)
            {
                continue;
            }

            is_valid = Utils::CommandLine::NumberParser::parse_option_unsigned(argument,
                                                                               in_cursor.has_next(),
                                                                               in_cursor.get_next_argument(),
                                                                               UINT64_MAX,
                                                                              &value,
                                                                               out_error_ptr_inner);

            if (is_valid)
            {
                number_option.set_func(value);

                is_known           = true;
                *out_uses_next_ptr = true;
            }
        }

        if (is_valid      &&
            is_known == false)
        {
            is_valid = in_cursor.ask(in_program_option_func,
                                    &is_known,
                                     out_uses_next_ptr,
                                     out_error_ptr_inner);
        }

        if (is_valid == false ||
            is_known)
        {
            /* Either the argument was not right, or it was an option and is done with. */
        }
        else
        if (argument.empty()  == false &&
            argument.at   (0) == '-')
        {
            *out_error_ptr_inner = "Unknown option " + argument;
            is_valid             = false;
        }
        else
        {
            is_valid = in_positional_func(argument,
                                          n_positionals,
                                          out_error_ptr_inner);

            ++n_positionals;
        }

        return is_valid;
    };

    return Utils::CommandLine::ArgumentWalker::walk(in_argc,
                                                    in_argv,
                                                    parse_argument_func,
                                                    out_error_ptr);
}
