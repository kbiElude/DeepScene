/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * What the programs that ask questions of a file of the analysis (APIDumpAnalysisQuery, APIDumpDependencyQuery, APIDumpTextureQuery) are asked to do in the same
 * way: print JSON on one line or not, check the workload or not, where the workload is, and how much of a list to print.
 */
#ifndef APIDUMPANALYSIS_QUERY_OPTIONS_H
#define APIDUMPANALYSIS_QUERY_OPTIONS_H

#include <functional>
#include <stddef.h>
#include <stdint.h>
#include <string>
#include <vector>

namespace Analysis
{
    namespace APIDumpAnalysis
    {
        /* An option of a program that takes a number and nothing else is done with it: its name (with the dashes), and what the program does with the number. */
        struct QueryNumberOption
        {
            std::string                           name;
            std::function<void(const uint64_t&)> set_func;
        };

        struct QueryOptions
        {
            bool        is_compact        = false;
            bool        is_verify_off     = false;
            uint64_t    limit             = 100; /* items in a list; 0 is all */
            uint64_t    offset            = 0;   /* items to skip             */
            std::string workload_filename;       /* where the workload is, if not where the file says */
        };

        /* What a query program says of the arguments that are not options: the file, the command, and then numbers. */
        struct QueryPositionalRules
        {
            size_t      max_n_numbers = 0; /* more of them are refused with "Too many arguments" */
            std::string missing_text;      /* said if the file or the command is not there       */
            std::string number_error_text; /* in front of the argument that is not a number      */
        };

        /* What the arguments that are not options said. */
        struct QueryPositionals
        {
            std::string           command;
            std::string           filename;
            std::vector<uint64_t> number_vec;
        };
    }
}

#endif /* APIDUMPANALYSIS_QUERY_OPTIONS_H */
