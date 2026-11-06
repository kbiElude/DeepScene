/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_analysis/frontends/analysis_session.h"
#include "analysis/api_dump_analysis/number_parser.h"
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

std::string Analysis::APIDumpDependencyExtractor::get_default_output_filename(const std::string& in_input_filename)
{
    const size_t extension_position = in_input_filename.find_last_of(".\\/");
    std::string  result             = in_input_filename;

    if (extension_position                       != std::string::npos &&
        in_input_filename.at(extension_position) == '.')
    {
        result.resize(extension_position);
    }

    return result + ".deps";
}

bool Analysis::APIDumpDependencyExtractor::parse_arguments(const int&   in_argc,
                                                           char*        in_argv[],
                                                           Options*     out_options_ptr,
                                                           std::string* out_error_ptr)
{
    /* More threads than this would only wait for the memory: reading is a matter of how much is moved. */
    constexpr uint32_t DEFAULT_MAX_N_THREADS = 8;
    constexpr uint64_t MAX_N_THREADS         = 64;

    bool result             = false;
    bool is_n_threads_given = false;

    for (int n_argument = 1;
             n_argument < in_argc;
           ++n_argument)
    {
        const std::string argument      = in_argv[n_argument];
        const bool        has_next      = (n_argument + 1 < in_argc);
        const std::string next_argument = (has_next) ? in_argv[n_argument + 1]
                                                     : "";

        if (argument == "-o")
        {
            if (has_next == false)
            {
                *out_error_ptr = "-o needs a file name";

                goto end;
            }

            out_options_ptr->output_filename = next_argument;

            ++n_argument;
        }
        else
        if (argument == "--max-frames")
        {
            if (has_next == false)
            {
                *out_error_ptr = "--max-frames needs a number";

                goto end;
            }

            if (Analysis::APIDumpAnalysis::NumberParser::parse_unsigned(next_argument,
                                                                        UINT64_MAX,
                                                                       &out_options_ptr->max_frames) == false)
            {
                *out_error_ptr = "--max-frames needs a number, not " + next_argument;

                goto end;
            }

            ++n_argument;
        }
        else
        if (argument == "--progress-interval")
        {
            if (has_next == false)
            {
                *out_error_ptr = "--progress-interval needs a number of seconds";

                goto end;
            }

            if (Analysis::APIDumpAnalysis::NumberParser::parse_double(next_argument,
                                                                     &out_options_ptr->progress_interval_seconds) == false)
            {
                *out_error_ptr = "--progress-interval needs a number of seconds, not " + next_argument;

                goto end;
            }

            ++n_argument;
        }
        else
        if (argument == "--skip-chunks-above")
        {
            uint64_t n_bytes = 0;

            if (has_next == false)
            {
                *out_error_ptr = "--skip-chunks-above needs a number of bytes";

                goto end;
            }

            if (Analysis::APIDumpAnalysis::NumberParser::parse_unsigned(next_argument,
                                                                        UINT32_MAX,
                                                                       &n_bytes) == false)
            {
                *out_error_ptr = "--skip-chunks-above needs a number of bytes, not " + next_argument;

                goto end;
            }

            out_options_ptr->skip_data_chunks_above_n_bytes = static_cast<uint32_t>(n_bytes);

            ++n_argument;
        }
        else
        if (argument == "--stats")
        {
            out_options_ptr->print_stats = true;
        }
        else
        if (argument == "--threads")
        {
            uint64_t n_threads = 0;

            if (has_next == false)
            {
                *out_error_ptr = "--threads needs a number";

                goto end;
            }

            if (Analysis::APIDumpAnalysis::NumberParser::parse_unsigned(next_argument,
                                                                        MAX_N_THREADS,
                                                                       &n_threads) == false)
            {
                *out_error_ptr = "--threads needs a number up to " + std::to_string(MAX_N_THREADS) + ", not " + next_argument;

                goto end;
            }

            out_options_ptr->n_threads = static_cast<uint32_t>(n_threads);
            is_n_threads_given         = true;

            ++n_argument;
        }
        else
        if (argument.empty() == false &&
            argument.at(0)   == '-')
        {
            *out_error_ptr = "Unknown option " + argument;

            goto end;
        }
        else
        if (out_options_ptr->input_filename.empty() )
        {
            out_options_ptr->input_filename = argument;
        }
        else
        {
            *out_error_ptr = "More than one workload given";

            goto end;
        }
    }

    if (out_options_ptr->input_filename.empty() )
    {
        *out_error_ptr = "No workload given";

        goto end;
    }

    if (out_options_ptr->output_filename.empty() )
    {
        out_options_ptr->output_filename = get_default_output_filename(out_options_ptr->input_filename);
    }

    if (is_n_threads_given == false)
    {
        out_options_ptr->n_threads = std::min<uint32_t>(DEFAULT_MAX_N_THREADS,
                                                        std::thread::hardware_concurrency() );
    }

    result = true;

end:
    return result;
}

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
    typedef std::chrono::steady_clock Clock;

    Analysis::APIDumpDependencies::AccessLog              log;
    Analysis::APIDumpDependencies::DependencyAnalysis     analysis;
    std::string                                           error;
    ExitCode                                              exit_code = ExitCode::SUCCESS;
    Options                                               options;
    const auto                                            start_time = Clock::now();

    /* Without history: the analysis keeps what it keeps of the frames only as far as the dependencies need it. */
    Analysis::APIDumpAnalysis::Frontends::AnalysisSession session(false);

    if (parse_arguments(in_argc,
                        in_argv,
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
        auto last_progress_time = start_time;

        const auto get_elapsed_seconds_func = [&]()
        {
            return std::chrono::duration<double>(Clock::now() - start_time).count();
        };

        const auto progress_func = [&](const Analysis::APIDumpAnalysis::Frontends::SessionProgress& in_progress)
        {
            const double since_last_seconds = std::chrono::duration<double>(Clock::now() - last_progress_time).count();

            if (since_last_seconds >= options.progress_interval_seconds)
            {
                last_progress_time = Clock::now();

                fprintf(stdout,
                        "[%7.1fs] segments=%llu calls=%llu frames=%llu demo=%.1fs\n",
                        get_elapsed_seconds_func(),
                        static_cast<unsigned long long>(in_progress.n_segments),
                        static_cast<unsigned long long>(in_progress.n_calls),
                        static_cast<unsigned long long>(in_progress.n_frames),
                        in_progress.demo_seconds);
                fflush(stdout);
            }
        };

        fprintf(stdout,
                "Reading %s\n",
                options.input_filename.c_str() );
        fflush(stdout);

        session.set_access_sink                  (&log);
        session.set_max_frames                   (options.max_frames);
        session.set_n_reader_threads             (options.n_threads);
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
            auto       model   = session.take_model();

            analysis.frame_call_range_vec = log.get_frame_call_range_vec();
            analysis.is_truncated         = summary.is_truncated;
            analysis.n_calls              = summary.n_calls;
            analysis.pointer_n_bytes      = summary.pointer_n_bytes;
            analysis.qpc_frequency        = summary.qpc_frequency;
            analysis.resource_vec         = std::move(model.resource_vec);
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
