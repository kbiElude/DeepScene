/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "redundant_state_counter_mode.h"
#include "analysis/api_dump_dependencies/deps_file.h"
#include "analysis/api_dump_dependencies/frame_slice_planner.h"
#include "capture/api_dump_names/function_names.h"
#include "frame_hash_sink.h"
#include "replay_runner.h"
#include "redundant_state_counter.h"
#include <stdio.h>

bool Stereo::Replay::Inspector::take_redundant_state_count(const Stereo::Replay::Inspector::Options& in_options,
                                                  std::string*                                       out_error_ptr)
{
    constexpr size_t N_FUNCTIONS_LISTED = 30;

    Analysis::APIDumpDependencies::DependencyFileContent analysis;
    bool                                                 result      = false;
    Stereo::Replay::Inspector::FrameHashSink sink(false); /* in_keeps_images */
    const bool                                        uses_recipe = (in_options.recipe_filename.empty() == false);

    if (in_options.deps_filename.empty() )
    {
        *out_error_ptr = "The census of the state needs --deps: the frames that a seek does not make are those that the dependencies say.";

        goto end;
    }

    if (Analysis::APIDumpDependencies::DepsFile::read(in_options.deps_filename,
                                                     &analysis,
                                                      out_error_ptr) == false)
    {
        goto end;
    }

    {
        const auto slice = Analysis::APIDumpDependencies::FrameSlicePlanner::plan(analysis,
                                                                                  in_options.last_frame);

        uint64_t n_dead_frames = 0;

        for (uint32_t n_frame = 0;
                      n_frame < in_options.last_frame;
                    ++n_frame)
        {
            if (Analysis::APIDumpDependencies::FrameSlicePlanner::is_live(slice,
                                                                           n_frame) == false)
            {
                ++n_dead_frames;
            }
        }

        Stereo::Replay::Inspector::RedundantStateCounter census(slice.is_live_vec);

        if (run_variant(in_options,
                        REPLAY_WAY_ARRAY[(uses_recipe) ? 4 : 0],
                       &sink,
                        out_error_ptr,
                       &census) == false)
        {
            goto end;
        }

        census.finish();

        const auto entry_vec = census.get_entries();

        uint64_t n_calls      = 0;
        uint64_t n_in_lists   = 0;
        uint64_t n_keyed      = 0;
        uint64_t n_overwritten = 0;
        uint64_t n_repeats    = 0;
        uint64_t n_skipped    = 0;

        for (const auto& entry : entry_vec)
        {
            n_calls       += entry.n_calls;
            n_in_lists    += entry.n_in_lists;
            n_keyed       += entry.n_keyed;
            n_overwritten += entry.n_overwritten_later;
            n_repeats     += entry.n_repeats;
            n_skipped     += entry.n_skipped_by_others;
        }

        fprintf(stdout,
                "Census of the state for a seek to frame %u%s: %llu of the %u frames before it are not made.\n"
                "\n"
                "Calls of OpenGL in those frames: %llu, of which %llu are made into lists (kept in them) and %llu were left out by the cache of texture parameters.\n"
                "Calls that give a piece of state a value: %llu.\n"
                "  that give it the value it has:                          %llu (a cache of the state leaves them out)\n"
                "  that another call overwrites before a frame that is made\n"
                "  or a barrier (only the last of a run need be made):     %llu, in %llu runs\n"
                "  that would still be made, if the last of each run is made only when\n"
                "  it gives a value other than the state had at the start of the run: %llu (%llu of the last ones give the value the state had)\n"
                "\n"
                "  %-30s %-9s %10s %10s %10s %10s %10s\n",
                in_options.last_frame,
                (uses_recipe) ? ", for the eyes of the recipe" : "",
                static_cast<unsigned long long>(n_dead_frames),
                in_options.last_frame,
                static_cast<unsigned long long>(n_calls),
                static_cast<unsigned long long>(n_in_lists),
                static_cast<unsigned long long>(n_skipped),
                static_cast<unsigned long long>(n_keyed),
                static_cast<unsigned long long>(n_repeats),
                static_cast<unsigned long long>(n_overwritten),
                static_cast<unsigned long long>(census.get_n_runs() ),
                static_cast<unsigned long long>(census.get_n_changed_keys() ),
                static_cast<unsigned long long>(census.get_n_unchanged_keys() ),
                "function",
                "class",
                "calls",
                "in lists",
                "keyed",
                "repeats",
                "overwritten");

        size_t n_listed = 0;

        for (const auto& entry : entry_vec)
        {
            const char* class_name_ptr = "other";

            switch (entry.call_class)
            {
                case Stereo::Replay::Inspector::StateCallClass::BARRIER:           class_name_ptr = "barrier";  break;
                case Stereo::Replay::Inspector::StateCallClass::KEYED:             class_name_ptr = "keyed";    break;
                case Stereo::Replay::Inspector::StateCallClass::OTHER:             class_name_ptr = "other";    break;
                case Stereo::Replay::Inspector::StateCallClass::SEQUENCE: class_name_ptr = "sequence"; break;

                default:
                {
                    break;
                }
            }

            if (n_listed++ < N_FUNCTIONS_LISTED)
            {
                fprintf(stdout,
                        "  %-30s %-9s %10llu %10llu %10llu %10llu %10llu\n",
                        Capture::APIDumpNames::get_function_name(entry.function),
                        class_name_ptr,
                        static_cast<unsigned long long>(entry.n_calls),
                        static_cast<unsigned long long>(entry.n_in_lists),
                        static_cast<unsigned long long>(entry.n_keyed),
                        static_cast<unsigned long long>(entry.n_repeats),
                        static_cast<unsigned long long>(entry.n_overwritten_later) );
            }
        }

        fprintf(stdout,
                "\nBarriers in those frames (they end a run, and what is known of the state):\n");

        for (const auto& barrier : census.get_barrier_count_map() )
        {
            fprintf(stdout,
                    "  %-30s %llu\n",
                    barrier.first.c_str(),
                    static_cast<unsigned long long>(barrier.second) );
        }

        fprintf(stdout,
                "\nFrames that are not made and have calls: %llu\n"
                "  every push of attributes and of a matrix has its pop:    %llu\n"
                "  and no state is given a value, and no matrix changed,\n"
                "  outside the pushes (the frame changes nothing that lasts): %llu\n"
                "  calls that give state a value outside a push of attributes: %llu\n"
                "  calls that change a matrix outside a push of one:           %llu\n"
                "\nMasks of the pushes of attributes in them:\n",
                static_cast<unsigned long long>(census.get_n_dead_frames_with_calls() ),
                static_cast<unsigned long long>(census.get_n_balanced_frames() ),
                static_cast<unsigned long long>(census.get_n_neutral_frames() ),
                static_cast<unsigned long long>(census.get_n_state_calls_outside_pushes() ),
                static_cast<unsigned long long>(census.get_n_matrix_calls_outside_pushes() ) );

        for (const auto& mask : census.get_push_mask_count_map() )
        {
            fprintf(stdout,
                    "  0x%llX  %llu\n",
                    static_cast<unsigned long long>(mask.first),
                    static_cast<unsigned long long>(mask.second) );
        }

        result = true;
    }

end:
    return result;
}
