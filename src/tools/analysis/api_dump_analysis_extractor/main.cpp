/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_analysis/extractor_command_line.h"
#include "analysis/api_dump_analysis/extractor_progress.h"
#include "analysis/api_dump_analysis/frontends/analysis_session.h"
#include "analysis/api_dump_analysis/passes_file.h"
#include "analysis/api_dump_analysis/resource_access_tracker.h"
#include "analysis/api_dump_analysis/timeline_builder.h"
#include "extractor.h"
#include <algorithm>
#include <chrono>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <thread>

void Analysis::APIDumpAnalysisExtractor::print_stats(const Analysis::APIDumpAnalysis::AnalysisResult& in_analysis)
{
    const auto access_count_map = Analysis::APIDumpAnalysis::ResourceAccessTracker::count_by_class(in_analysis.model.resource_access_vec);
    uint64_t   n_draws          = 0;
    const auto timeline         = Analysis::APIDumpAnalysis::TimelineBuilder::build               (in_analysis);

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
            static_cast<unsigned long long>(in_analysis.model.frame_shape_interner.get_n_structures   () ),
            static_cast<unsigned long long>(in_analysis.model.pass_structure_interner.get_n_structures() ),
            static_cast<unsigned long long>(in_analysis.model.draw_structure_map.size                 () ),
            static_cast<unsigned long long>(in_analysis.model.resource_vec.size                       () ),
            static_cast<unsigned long long>(access_count_map.at                                       (Analysis::APIDumpAnalysis::ResourceAccessClass::FRAME_LOCAL) ),
            static_cast<unsigned long long>(access_count_map.at                                       (Analysis::APIDumpAnalysis::ResourceAccessClass::LOOP_CARRIED) ),
            static_cast<unsigned long long>(access_count_map.at                                       (Analysis::APIDumpAnalysis::ResourceAccessClass::STATIC) ),
            static_cast<unsigned long long>(access_count_map.at                                       (Analysis::APIDumpAnalysis::ResourceAccessClass::UNREAD) ),
            static_cast<unsigned long long>(access_count_map.at                                       (Analysis::APIDumpAnalysis::ResourceAccessClass::UNUSED) ),
            static_cast<unsigned long long>(timeline.run_vec.size                                     () ),
            static_cast<unsigned long long>(timeline.setup_frame_index_vec.size                       () ),
            static_cast<unsigned long long>(in_analysis.model.n_key_collisions) );
}

void Analysis::APIDumpAnalysisExtractor::print_usage()
{
    fprintf(stdout,
            "Usage:\n"
            "\n"
            "APIDumpAnalysisExtractor drive:\\path\\to\\file.workload [-o file.passes] [--max-frames N]\n"
            "                         [--progress-interval SECONDS] [--skip-chunks-above BYTES] [--stats] [--threads N]\n"
            "\n"
            "Finds the render passes of a workload, and stores them in a .passes file for APIDumpAnalysisQuery.\n"
            "\n"
            "  -o                   The file to write (default: the workload's name with .passes)\n"
            "  --max-frames N       Stop after the Nth frame (default: all of them)\n"
            "  --progress-interval  Seconds between progress lines (default: 5; 0: after every segment)\n"
            "  --skip-chunks-above  Data chunks of more bytes than this (vertices, pixels) are not read (default: 0, all are read)\n"
            "  --stats              Print what was found when done\n"
            "  --threads N          Threads that read the workload ahead of the analysis (default: the cores of the machine, up to 8; 0: none)\n"
            "\n"
            "Exit codes: 0 done, 1 bad arguments, 2 workload cannot be read, 3 .passes cannot be written.\n");
}

int Analysis::APIDumpAnalysisExtractor::run(int   in_argc,
                                            char* in_argv[])
{
    Analysis::APIDumpAnalysis::AnalysisResult             analysis;
    std::string                                           error;
    ExitCode                                              exit_code  = ExitCode::SUCCESS;
    Analysis::APIDumpAnalysis::ExtractorOptions           options;
    Analysis::APIDumpAnalysis::ExtractorProfile           profile;
    Analysis::APIDumpAnalysis::Frontends::AnalysisSession session;

    profile.output_extension = ".passes";

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

        /* The shapes of the frames and the structures of the passes that have been found, which are what this program is for. */
        const auto extra_text_func = [](const Analysis::APIDumpAnalysis::Frontends::SessionProgress& in_progress)
        {
            char text_i8vec64[64] = {};

            ::snprintf(text_i8vec64,
                       sizeof(text_i8vec64),
                       " shapes=%llu passes=%llu",
                       static_cast<unsigned long long>(in_progress.n_frame_shapes),
                       static_cast<unsigned long long>(in_progress.n_pass_structures) );

            return std::string(text_i8vec64);
        };

        const auto progress_func = progress.get_progress_func(extra_text_func);

        fprintf(stdout,
                "Reading %s\n",
                options.input_filename.c_str() );
        fflush(stdout);

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
