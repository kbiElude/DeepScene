/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * The settings that the query programs (APIDumpAnalysisQuery, APIDumpDependencyQuery, APIDumpTextureQuery) share: whether to print JSON on
 * one line, whether to check the workload, where the workload is, and how much of a list to print.
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
        /* An option of a program that takes only a number: its name (with the dashes) and what the program does with the number. */
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
            std::string workload_filename;       /* the path of the workload, if it is not the one that the file names */
        };

        /* How a query program handles the arguments that are not options: the file, the command, and then numbers. */
        struct QueryPositionalRules
        {
            size_t      max_n_numbers = 0; /* more numbers than this are refused with "Too many arguments" */
            std::string missing_text;      /* the message if the file or the command is missing            */
            std::string number_error_text; /* the message prefix for an argument that is not a number      */
        };

        /* The parsed arguments that are not options. */
        struct QueryPositionals
        {
            std::string           command;
            std::string           filename;
            std::vector<uint64_t> number_vec;
        };
    }
}

#endif /* APIDUMPANALYSIS_QUERY_OPTIONS_H */
