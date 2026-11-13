/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_analysis/extractor_tool.h"
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
    Analysis::APIDumpAnalysis::Frontends::AnalysisSession session;

    Analysis::APIDumpAnalysis::ExtractorTool::Steps steps;

    /* The shapes of the frames and the structures of the passes that have been found, which are what this program is for. */
    steps.extra_text_func = [](const Analysis::APIDumpAnalysis::Frontends::SessionProgress& in_progress)
    {
        char text_i8vec64[64] = {};

        ::snprintf(text_i8vec64,
                   sizeof(text_i8vec64),
                   " shapes=%llu passes=%llu",
                   static_cast<unsigned long long>(in_progress.n_frame_shapes),
                   static_cast<unsigned long long>(in_progress.n_pass_structures) );

        return std::string(text_i8vec64);
    };

    steps.after_read_func = [&analysis](const Analysis::APIDumpAnalysis::ExtractorOptions&     in_options,
                                        Analysis::APIDumpAnalysis::Frontends::AnalysisSession* inout_session_ptr,
                                        const Analysis::APIDumpAnalysis::ExtractorProgress&    in_progress)
    {
        std::string                                  error;
        Analysis::APIDumpAnalysis::ExtractorExitCode exit_code = Analysis::APIDumpAnalysis::ExtractorExitCode::SUCCESS;

        analysis.model              = inout_session_ptr->take_model();
        analysis.summary            = inout_session_ptr->get_summary();
        analysis.workload_reference = inout_session_ptr->get_workload_reference();

        if (inout_session_ptr->has_reached_frame_limit() )
        {
            fprintf(stdout,
                    "Stopped at the limit of %llu frames.\n",
                    static_cast<unsigned long long>(in_options.max_frames) );
        }

        fprintf(stdout,
                "Analyzed %llu calls, %llu frames, %llu frame shapes, %llu pass structures in %.1fs\n",
                static_cast<unsigned long long>(analysis.summary.n_calls),
                static_cast<unsigned long long>(analysis.model.frame_record_vec.size() ),
                static_cast<unsigned long long>(analysis.model.frame_shape_interner.get_n_structures() ),
                static_cast<unsigned long long>(analysis.model.pass_structure_interner.get_n_structures() ),
                in_progress.get_elapsed_seconds() );

        if (Analysis::APIDumpAnalysis::PassesFile::write(in_options.output_filename,
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

    steps.print_usage_func         = print_usage;
    steps.profile.output_extension = ".passes";

    return static_cast<int>(Analysis::APIDumpAnalysis::ExtractorTool::run(in_argc,
                                                                          in_argv,
                                                                          steps,
                                                                         &session) );
}

int main(int   in_argc,
         char* in_argv[])
{
    return Analysis::APIDumpAnalysisExtractor::run(in_argc,
                                                   in_argv);
}
