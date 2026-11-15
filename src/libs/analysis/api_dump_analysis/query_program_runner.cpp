/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_analysis/query_program_runner.h"
#include "analysis/api_dump_analysis/workload_identity.h"
#include <stdio.h>

Analysis::APIDumpAnalysis::QueryExitCode Analysis::APIDumpAnalysis::QueryProgramRunner::run(const QueryOptions&  in_options,
                                                                                            const std::string&   in_filename,
                                                                                            const char*          in_file_noun_ptr,
                                                                                            const ReadFunc&      in_read_func,
                                                                                            const ReferenceFunc& in_reference_func,
                                                                                            const CommandFunc&   in_command_func)
{
    std::string   error;
    QueryExitCode exit_code = QueryExitCode::SUCCESS;

    if (in_read_func(in_filename,
                    &error) == false)
    {
        fprintf(stderr,
                "Error: %s\n",
                error.c_str() );

        exit_code = QueryExitCode::CANNOT_READ_FILE;

        goto end;
    }

    if (in_options.is_verify_off == false)
    {
        const WorkloadReference& reference = in_reference_func();
        const auto               match     = WorkloadIdentity::check_for_query(in_options.workload_filename,
                                                                               reference,
                                                                               in_file_noun_ptr);

        if (match == WorkloadMatch::MISMATCH)
        {
            exit_code = QueryExitCode::WORKLOAD_MISMATCH;

            goto end;
        }
    }

    {
        Utils::Json::JsonWriter writer(in_options.is_compact == false);
        const auto              status = in_command_func(&writer,
                                                         &error);

        if (status == QueryStatus::OK)
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

            exit_code = (status == QueryStatus::NOT_FOUND) ? QueryExitCode::NOT_FOUND
                                                           : QueryExitCode::BAD_ARGUMENTS;
        }
    }

end:
    return exit_code;
}
