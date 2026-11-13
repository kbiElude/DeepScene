/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_analysis/extractor_tool.h"
#include <stdio.h>
#include <string>

Analysis::APIDumpAnalysis::ExtractorExitCode Analysis::APIDumpAnalysis::ExtractorTool::run(const int&                  in_argc,
                                                                                           char*                       in_argv[],
                                                                                           const Steps&                in_steps,
                                                                                           Frontends::AnalysisSession* inout_session_ptr)
{
    std::string       error;
    ExtractorExitCode exit_code = ExtractorExitCode::SUCCESS;
    ExtractorOptions  options;

    if (ExtractorCommandLine::parse(in_argc,
                                    in_argv,
                                    in_steps.profile,
                                    in_steps.program_option_func,
                                   &options,
                                   &error) == false)
    {
        fprintf(stdout,
                "Error: %s\n\n",
                error.c_str() );

        in_steps.print_usage_func();

        exit_code = ExtractorExitCode::BAD_ARGUMENTS;

        goto end;
    }

    if (in_steps.before_read_func)
    {
        exit_code = in_steps.before_read_func(options);

        if (exit_code != ExtractorExitCode::SUCCESS)
        {
            goto end;
        }
    }

    {
        ExtractorProgress progress(options.progress_interval_seconds);

        const auto progress_func = progress.get_progress_func(in_steps.extra_text_func);

        fprintf(stdout,
                "Reading %s\n",
                options.input_filename.c_str() );
        fflush(stdout);

        inout_session_ptr->set_max_frames                    (options.max_frames);
        inout_session_ptr->set_n_reader_threads              (options.n_threads);
        inout_session_ptr->set_skip_data_chunks_above_n_bytes(options.skip_data_chunks_above_n_bytes);

        if (inout_session_ptr->run(options.input_filename.c_str(),
                                   progress_func,
                                  &error) == false)
        {
            fprintf(stdout,
                    "Error: %s\n",
                    error.c_str() );

            exit_code = ExtractorExitCode::CANNOT_READ_WORKLOAD;

            goto end;
        }

        if (inout_session_ptr->get_summary().is_truncated)
        {
            fprintf(stdout,
                    "Warning: the workload ends in the middle of a segment; it was analyzed as far as it goes.\n");
        }

        exit_code = in_steps.after_read_func(options,
                                             inout_session_ptr,
                                             progress);
    }

end:
    return exit_code;
}
