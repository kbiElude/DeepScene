/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_analysis/extractor_program_runner.h"
#include "analysis/api_dump_analysis/frontends/analysis_session.h"
#include "analysis/api_dump_dependencies/access_log.h"
#include "analysis/api_dump_dependencies/dependency_analyzer.h"
#include "analysis/api_dump_dependencies/deps_file.h"
#include "extractor.h"
#include <algorithm>
#include <chrono>
#include <map>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <thread>

void Analysis::APIDumpDependencyExtractor::print_stats(const Analysis::APIDumpDependencies::DependencyFileContent& in_analysis)
{
    std::map<Analysis::APIDumpDependencies::DependencyClass, uint64_t> n_resources_by_class_map;

    for (const auto& dependencies : in_analysis.result.resource_vec)
    {
        n_resources_by_class_map[dependencies.summary.dependency_class]++;
    }

    fprintf(stdout,
            "Calls:               %llu\n"
            "Frames:              %llu\n"
            "Resources:           %llu\n"
            "  carried:           %llu\n"
            "  carried possibly:  %llu\n"
            "  frame local:       %llu\n"
            "  static:            %llu\n"
            "  unused:            %llu\n",
            static_cast<unsigned long long>(in_analysis.n_calls),
            static_cast<unsigned long long>(in_analysis.result.n_frames),
            static_cast<unsigned long long>(in_analysis.resource_vec.size() ),
            static_cast<unsigned long long>(n_resources_by_class_map[Analysis::APIDumpDependencies::DependencyClass::CARRIED]),
            static_cast<unsigned long long>(n_resources_by_class_map[Analysis::APIDumpDependencies::DependencyClass::CARRIED_POSSIBLY]),
            static_cast<unsigned long long>(n_resources_by_class_map[Analysis::APIDumpDependencies::DependencyClass::FRAME_LOCAL]),
            static_cast<unsigned long long>(n_resources_by_class_map[Analysis::APIDumpDependencies::DependencyClass::STATIC]),
            static_cast<unsigned long long>(n_resources_by_class_map[Analysis::APIDumpDependencies::DependencyClass::UNUSED]) );
}

void Analysis::APIDumpDependencyExtractor::print_usage()
{
    fprintf(stdout,
            "Usage:\n"
            "\n"
            "APIDumpDependencyExtractor drive:\\path\\to\\file.workload [-o file.deps] [--max-frames N]\n"
            "                           [--progress-interval SECONDS] [--skip-chunks-above BYTES] [--stats] [--threads N]\n"
            "\n"
            "Finds what each frame of a workload does with what the frames before it left in the resources, and stores it in a .deps file for\n"
            "APIDumpDependencyQuery.\n"
            "\n"
            "  -o                   The file to write (default: the workload's name with .deps)\n"
            "  --max-frames N       Stop after the Nth frame (default: all of them)\n"
            "  --progress-interval  Seconds between progress lines (default: 5; 0: after every segment)\n"
            "  --skip-chunks-above  Data chunks of more bytes than this (vertices, pixels) are not read (default: 65536; 0: all are read)\n"
            "  --stats              Print what was found when done\n"
            "  --threads N          Threads that read the workload ahead of the analysis and work out the dependencies (default: the cores of the\n"
            "                       machine, up to 8; 0: none)\n"
            "\n"
            "Exit codes: 0 done, 1 bad arguments, 2 workload cannot be read, 3 .deps cannot be written.\n");
}

int Analysis::APIDumpDependencyExtractor::run(int   in_argc,
                                              char* in_argv[])
{
    Analysis::APIDumpDependencies::AccessLog             log;
    Analysis::APIDumpDependencies::DependencyFileContent analysis;

    /* Without history: the analysis keeps only as much of each frame as the dependencies need. */
    Analysis::APIDumpAnalysis::Frontends::AnalysisSession session(false);

    Analysis::APIDumpAnalysis::ExtractorProgramRunner::Steps steps;

    session.set_access_sink(&log);

    steps.after_read_func = [&analysis, &log](const Analysis::APIDumpAnalysis::ExtractorOptions&     in_options,
                                              Analysis::APIDumpAnalysis::Frontends::AnalysisSession* inout_session_ptr,
                                              const Analysis::APIDumpAnalysis::ExtractorProgress&    in_progress)
    {
        std::string                                  error;
        Analysis::APIDumpAnalysis::ExtractorExitCode exit_code = Analysis::APIDumpAnalysis::ExtractorExitCode::SUCCESS;

        const double read_seconds = in_progress.get_elapsed_seconds();

        {
            const auto summary = inout_session_ptr->get_summary();
            auto       model   = inout_session_ptr->take_model ();

            analysis.frame_call_range_vec = log.get_frame_call_range_vec();
            analysis.is_truncated         = summary.is_truncated;
            analysis.n_calls              = summary.n_calls;
            analysis.pointer_n_bytes      = summary.pointer_n_bytes;
            analysis.qpc_frequency        = summary.qpc_frequency;
            analysis.resource_vec         = std::move                     (model.resource_vec);
            analysis.workload_reference   = inout_session_ptr->get_workload_reference();

            for (const auto& group : summary.n_calls_by_group_map)
            {
                analysis.n_calls_by_api_map[Capture::APIDumpLoader::get_api_group_name(group.first)] = group.second;
            }
        }

        analysis.result = Analysis::APIDumpDependencies::DependencyAnalyzer::analyze(log,
                                                                                     analysis.resource_vec,
                                                                                     in_options.n_threads);

        const double analyzed_seconds = in_progress.get_elapsed_seconds();

        fprintf(stdout,
                "Read %llu calls, %llu frames, %llu accesses in %.1fs; worked out the dependencies of %llu resources in %.1fs\n",
                static_cast<unsigned long long>(analysis.n_calls),
                static_cast<unsigned long long>(analysis.result.n_frames),
                static_cast<unsigned long long>(log.get_event_vec().size() ),
                read_seconds,
                static_cast<unsigned long long>(analysis.resource_vec.size() ),
                analyzed_seconds - read_seconds);

        if (Analysis::APIDumpDependencies::DepsFile::write(in_options.output_filename,
                                                           analysis,
                                                          &error) == false)
        {
            fprintf(stdout,
                    "Error: %s\n",
                    error.c_str() );

            exit_code = Analysis::APIDumpAnalysis::ExtractorExitCode::CANNOT_WRITE_OUTPUT;

            goto end;
        }

        fprintf(stdout,
                "Wrote %s\n",
                in_options.output_filename.c_str() );

        if (in_options.print_stats)
        {
            print_stats(analysis);
        }

end:
        return exit_code;
    };

    steps.print_usage_func = print_usage;

    /* The dependencies contain no pixels or vertices, so data chunks above 64 KiB are skipped by default. */
    steps.profile.default_skip_data_chunks_above_n_bytes = 64 * 1024;
    steps.profile.output_extension                       = ".deps";

    return static_cast<int>(Analysis::APIDumpAnalysis::ExtractorProgramRunner::run(in_argc,
                                                                                   in_argv,
                                                                                   steps,
                                                                                  &session) );
}

int main(int   in_argc,
         char* in_argv[])
{
    return Analysis::APIDumpDependencyExtractor::run(in_argc,
                                                     in_argv);
}
