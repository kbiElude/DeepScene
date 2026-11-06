/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "Analysis/APIDumpAnalysis/frontends/analysis_session.h"
#include "Analysis/APIDumpAnalysis/number_parser.h"
#include "Analysis/APIDumpAnalysis/passes_file.h"
#include "Analysis/APIDumpAnalysis/resource_access_tracker.h"
#include "Analysis/APIDumpAnalysis/timeline_builder.h"
#include "extractor.h"
#include <algorithm>
#include <chrono>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <thread>

std::string Analysis::APIDumpAnalysisExtractor::get_default_output_filename(const std::string& in_input_filename)
{
    const size_t extension_position = in_input_filename.find_last_of(".\\/");
    std::string  result             = in_input_filename;

    if (extension_position                       != std::string::npos &&
        in_input_filename.at(extension_position) == '.')
    {
        result.resize(extension_position);
    }

    return result + ".passes";
}

bool Analysis::APIDumpAnalysisExtractor::parse_arguments(const int&   in_argc,
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
        if (argument == "--stats")
        {
            out_options_ptr->print_stats = true;
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

void Analysis::APIDumpAnalysisExtractor::print_stats(const Analysis::APIDumpAnalysis::AnalysisResult& in_analysis)
{
    const auto access_count_map = Analysis::APIDumpAnalysis::ResourceAccessTracker::count_by_class(in_analysis.model.resource_access_vec);
    uint64_t   n_draws          = 0;
    const auto timeline         = Analysis::APIDumpAnalysis::TimelineBuilder::build(in_analysis);

    for (const auto& frame : in_analysis.model.frame_record_vec)
    {
        n_draws += frame.n_draws;
    }

    fprintf(stdout,
            "Calls:               %llu\n"
            "  ignored:           %llu (APIs without a front end)\n"
            "  unfollowed:        %llu\n"
            "  unsupported:       %llu\n"
            "  after last frame:  %llu\n"
            "Segments:            %llu\n"
            "Frames:              %llu\n"
            "Draws:               %llu\n"
            "Frame shapes:        %llu\n"
            "Pass structures:     %llu\n"
            "Draw structures:     %llu\n"
            "Resources:           %llu\n"
            "  frame local:       %llu\n"
            "  loop carried:      %llu\n"
            "  static:            %llu\n"
            "  unread:            %llu\n"
            "  unused:            %llu\n"
            "Runs:                %llu\n"
            "Setup frames:        %llu\n"
            "Key collisions:      %llu\n",
            static_cast<unsigned long long>(in_analysis.summary.n_calls),
            static_cast<unsigned long long>(in_analysis.summary.n_calls_ignored),
            static_cast<unsigned long long>(in_analysis.summary.n_unfollowed_calls),
            static_cast<unsigned long long>(in_analysis.summary.n_unsupported_calls),
            static_cast<unsigned long long>(in_analysis.summary.n_trailing_calls),
            static_cast<unsigned long long>(in_analysis.summary.n_segments),
            static_cast<unsigned long long>(in_analysis.model.frame_record_vec.size() ),
            static_cast<unsigned long long>(n_draws),
            static_cast<unsigned long long>(in_analysis.model.frame_shape_interner.get_n_structures() ),
            static_cast<unsigned long long>(in_analysis.model.pass_structure_interner.get_n_structures() ),
            static_cast<unsigned long long>(in_analysis.model.draw_structure_map.size() ),
            static_cast<unsigned long long>(in_analysis.model.resource_vec.size() ),
            static_cast<unsigned long long>(access_count_map.at(Analysis::APIDumpAnalysis::ResourceAccessClass::FRAME_LOCAL) ),
            static_cast<unsigned long long>(access_count_map.at(Analysis::APIDumpAnalysis::ResourceAccessClass::LOOP_CARRIED) ),
            static_cast<unsigned long long>(access_count_map.at(Analysis::APIDumpAnalysis::ResourceAccessClass::STATIC) ),
            static_cast<unsigned long long>(access_count_map.at(Analysis::APIDumpAnalysis::ResourceAccessClass::UNREAD) ),
            static_cast<unsigned long long>(access_count_map.at(Analysis::APIDumpAnalysis::ResourceAccessClass::UNUSED) ),
            static_cast<unsigned long long>(timeline.run_vec.size() ),
            static_cast<unsigned long long>(timeline.setup_frame_index_vec.size() ),
            static_cast<unsigned long long>(in_analysis.model.n_key_collisions) );
}

void Analysis::APIDumpAnalysisExtractor::print_usage()
{
    fprintf(stdout,
            "Usage:\n"
            "\n"
            "APIDumpAnalysisExtractor drive:\\path\\to\\file.workload [-o file.passes] [--max-frames N]\n"
            "                         [--progress-interval SECONDS] [--stats] [--threads N]\n"
            "\n"
            "Finds the render passes of a workload, and stores them in a .passes file for APIDumpAnalysisQuery.\n"
            "\n"
            "  -o                   The file to write (default: the workload's name with .passes)\n"
            "  --max-frames N       Stop after the Nth frame (default: all of them)\n"
            "  --progress-interval  Seconds between progress lines (default: 5; 0: after every segment)\n"
            "  --stats              Print what was found when done\n"
            "  --threads N          Threads that read the workload ahead of the analysis (default: the cores of the machine, up to 8; 0: none)\n"
            "\n"
            "Exit codes: 0 done, 1 bad arguments, 2 workload cannot be read, 3 .passes cannot be written.\n");
}

int Analysis::APIDumpAnalysisExtractor::run(int   in_argc,
                                            char* in_argv[])
{
    typedef std::chrono::steady_clock Clock;

    Analysis::APIDumpAnalysis::AnalysisResult                   analysis;
    std::string                                           error;
    ExitCode                                              exit_code = ExitCode::SUCCESS;
    Options                                               options;
    Analysis::APIDumpAnalysis::Frontends::AnalysisSession session;
    const auto                                            start_time = Clock::now();

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
                        "[%7.1fs] segments=%llu calls=%llu frames=%llu shapes=%llu passes=%llu demo=%.1fs\n",
                        get_elapsed_seconds_func(),
                        static_cast<unsigned long long>(in_progress.n_segments),
                        static_cast<unsigned long long>(in_progress.n_calls),
                        static_cast<unsigned long long>(in_progress.n_frames),
                        static_cast<unsigned long long>(in_progress.n_frame_shapes),
                        static_cast<unsigned long long>(in_progress.n_pass_structures),
                        in_progress.demo_seconds);
                fflush(stdout);
            }
        };

        fprintf(stdout,
                "Reading %s\n",
                options.input_filename.c_str() );
        fflush(stdout);

        session.set_max_frames      (options.max_frames);
        session.set_n_reader_threads(options.n_threads);

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

        analysis.model              = session.take_model();
        analysis.summary            = session.get_summary();
        analysis.workload_reference = session.get_workload_reference();

        if (analysis.summary.is_truncated)
        {
            fprintf(stdout,
                    "Warning: the workload ends in the middle of a segment; it was analyzed as far as it goes.\n");
        }

        if (session.has_reached_frame_limit() )
        {
            fprintf(stdout,
                    "Stopped at the limit of %llu frames.\n",
                    static_cast<unsigned long long>(options.max_frames) );
        }

        fprintf(stdout,
                "Analyzed %llu calls, %llu frames, %llu frame shapes, %llu pass structures in %.1fs\n",
                static_cast<unsigned long long>(analysis.summary.n_calls),
                static_cast<unsigned long long>(analysis.model.frame_record_vec.size() ),
                static_cast<unsigned long long>(analysis.model.frame_shape_interner.get_n_structures() ),
                static_cast<unsigned long long>(analysis.model.pass_structure_interner.get_n_structures() ),
                get_elapsed_seconds_func() );

        if (Analysis::APIDumpAnalysis::PassesFile::write(options.output_filename,
                                                         analysis,
                                                        &error) == false)
        {
            fprintf(stdout,
                    "Error: %s\n",
                    error.c_str() );

            exit_code = ExitCode::CANNOT_WRITE_PASSES;

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
    return Analysis::APIDumpAnalysisExtractor::run(in_argc,
                                                   in_argv);
}
