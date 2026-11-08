/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_analysis/extractor_command_line.h"
#include "analysis/api_dump_analysis/extractor_progress.h"
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

void Analysis::APIDumpDependencyExtractor::print_stats(const Analysis::APIDumpDependencies::DependencyAnalysis& in_analysis)
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
    Analysis::APIDumpDependencies::AccessLog          log;
    Analysis::APIDumpDependencies::DependencyAnalysis analysis;
    std::string                                       error;
    ExitCode                                          exit_code  = ExitCode::SUCCESS;
    Analysis::APIDumpAnalysis::ExtractorOptions       options;
    Analysis::APIDumpAnalysis::ExtractorProfile       profile;

    /* Without history: the analysis keeps what it keeps of the frames only as far as the dependencies need it. */
    Analysis::APIDumpAnalysis::Frontends::AnalysisSession session(false);

    /* Pixels and vertices are not what the dependencies are made of. */
    profile.default_skip_data_chunks_above_n_bytes = 64 * 1024;
    profile.output_extension                       = ".deps";

    if (Analysis::APIDumpAnalysis::ExtractorCommandLine::parse(in_argc,
                                                               in_argv,
                                                               profile,
                                                               Analysis::APIDumpAnalysis::ExtractorCommandLine::ProgramOptionFunc(),
                                                              &options,
                                                              &error) == false)
    {
        fprintf(stdout,
                "Error: %s\n\n",
                error.c_str() );

        print_usage();

        exit_code = ExitCode::BAD_ARGUMENTS;

        goto end;
    }

    {
        Analysis::APIDumpAnalysis::ExtractorProgress progress(options.progress_interval_seconds);

        const auto get_elapsed_seconds_func = [&]()
        {
            return progress.get_elapsed_seconds();
        };

        const auto progress_func = progress.get_progress_func();

        fprintf(stdout,
                "Reading %s\n",
                options.input_filename.c_str() );
        fflush(stdout);

        session.set_access_sink                   (&log);
        session.set_max_frames                    (options.max_frames);
        session.set_n_reader_threads              (options.n_threads);
        session.set_skip_data_chunks_above_n_bytes(options.skip_data_chunks_above_n_bytes);

        if (session.run(options.input_filename.c_str(),
                        progress_func,
                       &error) == false)
        {
            fprintf(stdout,
                    "Error: %s\n",
                    error.c_str() );

            exit_code = ExitCode::CANNOT_READ_WORKLOAD;

            goto end;
        }

        const double read_seconds = get_elapsed_seconds_func();

        {
            const auto summary = session.get_summary();
            auto       model   = session.take_model ();

            analysis.frame_call_range_vec = log.get_frame_call_range_vec();
            analysis.is_truncated         = summary.is_truncated;
            analysis.n_calls              = summary.n_calls;
            analysis.pointer_n_bytes      = summary.pointer_n_bytes;
            analysis.qpc_frequency        = summary.qpc_frequency;
            analysis.resource_vec         = std::move                     (model.resource_vec);
            analysis.workload_reference   = session.get_workload_reference();

            for (const auto& group : summary.n_calls_by_group_map)
            {
                analysis.n_calls_by_api_map[Capture::APIDumpLoader::get_api_group_name(group.first)] = group.second;
            }
        }

        analysis.result = Analysis::APIDumpDependencies::DependencyAnalyzer::analyze(log,
                                                                                     analysis.resource_vec,
                                                                                     options.n_threads);

        const double analyzed_seconds = get_elapsed_seconds_func();

        fprintf(stdout,
                "Read %llu calls, %llu frames, %llu accesses in %.1fs; worked out the dependencies of %llu resources in %.1fs\n",
                static_cast<unsigned long long>(analysis.n_calls),
                static_cast<unsigned long long>(analysis.result.n_frames),
                static_cast<unsigned long long>(log.get_event_vec().size() ),
                read_seconds,
                static_cast<unsigned long long>(analysis.resource_vec.size() ),
                analyzed_seconds - read_seconds);

        if (analysis.is_truncated)
        {
            fprintf(stdout,
                    "Warning: the workload ends in the middle of a segment; it was analyzed as far as it goes.\n");
        }

        if (Analysis::APIDumpDependencies::DepsFile::write(options.output_filename,
                                                           analysis,
                                                          &error) == false)
        {
            fprintf(stdout,
                    "Error: %s\n",
                    error.c_str() );

            exit_code = ExitCode::CANNOT_WRITE_DEPS;

            goto end;
        }

        fprintf(stdout,
                "Wrote %s\n",
                options.output_filename.c_str() );

        if (options.print_stats)
        {
            print_stats(analysis);
        }
    }

end:
    return static_cast<int>(exit_code);
}

int main(int   in_argc,
         char* in_argv[])
{
    return Analysis::APIDumpDependencyExtractor::run(in_argc,
                                                     in_argv);
}
