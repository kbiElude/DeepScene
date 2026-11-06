/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Writes the first frames of a workload as a workload of their own (see command_line.cpp for the arguments, and HeadSlicer).
 */
#include "command_line.h"
#include "Analysis/APIDumpSlicing/head_slicer.h"
#include <stdio.h>
#include <string>
#include <vector>

int main(int   in_argc,
         char* in_argv[])
{
    Analysis::APIDumpSlicer::ExitCode       exit_code = Analysis::APIDumpSlicer::ExitCode::SUCCESS;
    std::string                             error;
    Analysis::APIDumpSlicer::Options        options;
    Analysis::APIDumpSlicing::SliceReport   report;

    if (Analysis::APIDumpSlicer::CommandLine::parse(std::vector<std::string>(in_argv + 1,
                                                                             in_argv + in_argc),
                                                   &options,
                                                   &error) == false)
    {
        fprintf(stdout,
                "%s\n\n%s",
                error.c_str(),
                Analysis::APIDumpSlicer::CommandLine::get_usage_ptr() );

        exit_code = Analysis::APIDumpSlicer::ExitCode::BAD_ARGUMENTS;

        goto end;
    }

    if (Analysis::APIDumpSlicing::HeadSlicer::slice(options.workload_filename,
                                                    options.slice_filename,
                                                    options.slice_options,
                                                   &report,
                                                   &error) == false)
    {
        fprintf(stdout,
                "%s\n",
                error.c_str() );

        exit_code = Analysis::APIDumpSlicer::ExitCode::SLICE_FAILED;

        goto end;
    }

    fprintf(stdout,
            "Frames 0 to %llu of %s are in %s: %llu calls in %llu frames, %llu bytes. %u segments are copied as they are, %u are written again.\n",
            static_cast<unsigned long long>(options.slice_options.last_frame),
            options.workload_filename.c_str(),
            options.slice_filename.c_str(),
            static_cast<unsigned long long>(report.n_calls_written),
            static_cast<unsigned long long>(report.n_frames_written),
            static_cast<unsigned long long>(report.n_bytes_written),
            report.n_segments_copied,
            report.n_segments_rewritten);

    if (options.slice_options.strip_chunks_above_n_bytes != 0)
    {
        fprintf(stdout,
                "The data chunks of more than %u bytes are empty in it: it is for analysing, not for a replay.\n",
                options.slice_options.strip_chunks_above_n_bytes);
    }

end:
    return static_cast<int>(exit_code);
}
