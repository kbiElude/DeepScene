/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * The steps that the query programs (APIDumpAnalysisQuery, APIDumpDependencyQuery, APIDumpTextureQuery) share once their arguments are read:
 * read the file, check the workload that it belongs to, ask the question, and print the answer or the error. A program only supplies how its
 * file is read, which workload the file names, and what the question is.
 */
#ifndef APIDUMPANALYSIS_QUERY_TOOL_H
#define APIDUMPANALYSIS_QUERY_TOOL_H

#include "analysis/api_dump_analysis/query_options.h"
#include "analysis/api_dump_analysis/types.h"
#include "utils/json/json_writer.h"
#include <functional>
#include <string>

namespace Analysis
{
    namespace APIDumpAnalysis
    {
        class QueryTool
        {
        public:
            /* Asks the question and writes the answer to the writer. */
            typedef std::function<QueryStatus(Utils::Json::JsonWriter* in_writer_ptr,
                                              std::string*             out_error_ptr)> CommandFunc;

            /* Reads the file. Returns false and the reason if it cannot be read. */
            typedef std::function<bool(const std::string& in_filename,
                                       std::string*       out_error_ptr)> ReadFunc;

            /* The workload that the file read by the ReadFunc belongs to. */
            typedef std::function<const WorkloadReference&()> ReferenceFunc;

            /* The file noun is what the file is called in the message about a workload mismatch ("file", "analysis"). Prints the error or the answer. */
            static QueryExitCode run(const QueryOptions&  in_options,
                                     const std::string&   in_filename,
                                     const char*          in_file_noun_ptr,
                                     const ReadFunc&      in_read_func,
                                     const ReferenceFunc& in_reference_func,
                                     const CommandFunc&   in_command_func);
        };
    }
}

#endif /* APIDUMPANALYSIS_QUERY_TOOL_H */
