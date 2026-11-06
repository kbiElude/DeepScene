/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "profile_mode.h"
#include "Capture/APIDumpNames/function_names.h"
#include "frame_hash_sink.h"
#include "replay_profiler.h"
#include "replay_runner.h"
#include <stdio.h>

bool Stereo::Replay::ConsistencyChecker::take_profile(const Stereo::Replay::ConsistencyChecker::Options& in_options,
                                                      std::string*                                       out_error_ptr)
{
    constexpr size_t   N_FRAMES_LISTED    = 10;
    constexpr size_t   N_FUNCTIONS_LISTED = 15;
    constexpr double   NANOSECONDS_PER_MILLISECOND = 1000000.0;
    constexpr double   NANOSECONDS_PER_MICROSECOND = 1000.0;

    Stereo::Replay::ConsistencyChecker::ReplayProfiler profiler;
    bool                                               result   = false;
    Stereo::Replay::ConsistencyChecker::FrameHashSink  sink(false); /* in_keeps_images */
    const bool                                         uses_recipe = (in_options.recipe_filename.empty() == false);

    if (run_variant(in_options,
                    VARIANT_ARRAY[(uses_recipe) ? 4 : 0],
                   &sink,
                    out_error_ptr,
                   &profiler) == false)
    {
        goto end;
    }

    fprintf(stdout,
            "Replay of frames %u to %u%s: %llu calls took %.1f ms in all (the time of the calls, without the waiting for the pace of the capture).\n"
            "\n"
            "Functions by the time of their calls:\n"
            "  %10s %10s %12s %10s  %s\n",
            in_options.first_frame,
            in_options.last_frame,
            (uses_recipe) ? ", for the eyes of the recipe" : "",
            static_cast<unsigned long long>(profiler.get_n_calls() ),
            static_cast<double>(profiler.get_total_n_nanoseconds() ) / NANOSECONDS_PER_MILLISECOND,
            "total ms",
            "calls",
            "average us",
            "slowest us",
            "function");

    for (const auto& entry : profiler.get_slowest_functions(N_FUNCTIONS_LISTED) )
    {
        const char* function_name_ptr = Capture::APIDumpNames::get_function_name(entry.function);

        fprintf(stdout,
                "  %10.1f %10llu %12.2f %10.1f  %s\n",
                static_cast<double>(entry.total_n_nanoseconds) / NANOSECONDS_PER_MILLISECOND,
                static_cast<unsigned long long>(entry.n_calls),
                static_cast<double>(entry.total_n_nanoseconds) / static_cast<double>(entry.n_calls) / NANOSECONDS_PER_MICROSECOND,
                static_cast<double>(entry.max_n_nanoseconds) / NANOSECONDS_PER_MICROSECOND,
                function_name_ptr);
    }

    fprintf(stdout,
            "\nFrames that took longest:\n");

    for (const auto& entry : profiler.get_slowest_frames(N_FRAMES_LISTED) )
    {
        const char* slowest_function_name_ptr = Capture::APIDumpNames::get_function_name(entry.slowest_function);

        fprintf(stdout,
                "  frame %6u: %8.1f ms in %llu calls; the slowest call is %s (%.1f ms)\n",
                entry.n_frame,
                static_cast<double>(entry.total_n_nanoseconds) / NANOSECONDS_PER_MILLISECOND,
                static_cast<unsigned long long>(entry.n_calls),
                slowest_function_name_ptr,
                static_cast<double>(entry.slowest_n_nanoseconds) / NANOSECONDS_PER_MILLISECOND);
    }

    result = true;
end:
    return result;
}
