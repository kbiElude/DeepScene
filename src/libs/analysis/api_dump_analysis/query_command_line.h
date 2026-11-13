/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Reads the command line of the programs that ask questions of a file of the analysis (APIDumpAnalysisQuery, APIDumpDependencyQuery, APIDumpTextureQuery): --compact,
 * --limit, --no-verify, --offset and --workload are the same for all of them (see QueryOptions). A program is asked about each argument that is not one of these
 * before it is taken for an unknown option, and about each argument that is not an option at all (the file, the command, the numbers).
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
            /* What a program with options of its own says of an argument. */
            typedef Utils::CommandLine::ProgramOptionFunc ProgramOptionFunc;

            /* What a program says of an argument that is not an option: the nth of them (from 0). False, and the reason, if it is not right. */
            typedef std::function<bool(const std::string& in_argument,
                                       const size_t&      in_n_positional,
                                       std::string*       out_error_ptr)> PositionalFunc;

            /* False, and the reason, if the arguments are not right. The number options are those that only take a number: the program is asked about the others. */
            static bool parse(const int&                            in_argc,
                              char*                                 in_argv[],
                              const std::vector<QueryNumberOption>& in_number_option_vec,
                              const ProgramOptionFunc&              in_program_option_func,
                              const PositionalFunc&                 in_positional_func,
                              QueryOptions*                         out_options_ptr,
                              std::string*                          out_error_ptr);

            /* The same, for the programs whose arguments that are not options are a file, a command, and numbers (as many as the rules allow): the program is asked
             * about none of them, and gets what they said. False, and the reason, if they are not right, or if the file or the command is not there.
             */
            static bool parse_file_command_and_numbers(const int&                            in_argc,
                                                       char*                                 in_argv[],
                                                       const std::vector<QueryNumberOption>& in_number_option_vec,
                                                       const ProgramOptionFunc&              in_program_option_func,
                                                       const QueryPositionalRules&           in_rules,
                                                       QueryOptions*                         out_options_ptr,
                                                       QueryPositionals*                     out_positionals_ptr,
                                                       std::string*                          out_error_ptr);
        };
    }
}

#endif /* APIDUMPANALYSIS_QUERY_COMMAND_LINE_H */
