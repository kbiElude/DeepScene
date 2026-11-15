/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Reads the command line of the programs that query a file of the analysis (APIDumpAnalysisQuery, APIDumpDependencyQuery,
 * APIDumpTextureQuery). The options --compact, --limit, --no-verify, --offset and --workload are the same for all of them (see QueryOptions).
 * An argument that is none of these options is first offered to the program, and only then treated as an unknown option. An argument that is
 * not an option at all (the file, the command, the numbers) is also offered to the program.
 */
#ifndef APIDUMPANALYSIS_QUERY_COMMAND_LINE_H
#define APIDUMPANALYSIS_QUERY_COMMAND_LINE_H

#include "analysis/api_dump_analysis/query_options.h"
#include "utils/command_line/program_option_func.h"
#include <functional>
#include <stddef.h>
#include <string>
#include <vector>

namespace Analysis
{
    namespace APIDumpAnalysis
    {
        class QueryCommandLine
        {
        public:
            /* What a program with options of its own does with an argument. */
            typedef Utils::CommandLine::ProgramOptionFunc ProgramOptionFunc;

            /* What a program does with an argument that is not an option; in_n_positional counts these arguments from 0. Returns false and the reason if the argument is invalid. */
            typedef std::function<bool(const std::string& in_argument,
                                       const size_t&      in_n_positional,
                                       std::string*       out_error_ptr)> PositionalFunc;

            /* Returns false and the reason if the arguments are invalid. The number options are the options that only take a number; the program is asked about all other options. */
            static bool parse(const int&                            in_argc,
                              char*                                 in_argv[],
                              const std::vector<QueryNumberOption>& in_number_option_vec,
                              const ProgramOptionFunc&              in_program_option_func,
                              const PositionalFunc&                 in_positional_func,
                              QueryOptions*                         out_options_ptr,
                              std::string*                          out_error_ptr);

            /* Like parse(), for the programs whose non-option arguments are a file, a command, and numbers (as many as the rules allow). The
             * program is not asked about these arguments; it gets the parsed result. Returns false and the reason if they are invalid, or if the file or the command is missing.
             */
            static bool parse_file_command_and_numbers(const int&                            in_argc,
                                                       char*                                 in_argv[],
                                                       const std::vector<QueryNumberOption>& in_number_option_vec,
                                                       const ProgramOptionFunc&              in_program_option_func,
                                                       const QueryPositionalRules&           in_rules,
                                                       QueryOptions*                         out_options_ptr,
                                                       QueryPositionals*                     out_positionals_ptr,
                                                       std::string*                          out_error_ptr);

        private:
            /* Handles the argument if it is one of the options that every query program has. */
            static bool parse_common_option(const std::string& in_argument,
                                            const bool&        in_has_next,
                                            const std::string& in_next_argument,
                                            QueryOptions*      out_options_ptr,
                                            bool*              out_is_known_ptr,
                                            bool*              out_uses_next_ptr,
                                            std::string*       out_error_ptr);
        };
    }
}

#endif /* APIDUMPANALYSIS_QUERY_COMMAND_LINE_H */
