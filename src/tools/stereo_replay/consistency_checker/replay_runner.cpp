/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "replay_runner.h"
#include "stop_observer.h"
#include "tools/stereo_replay/engine_dll/entrypoints.h"
#include <chrono>
#include <stdio.h>
#include <thread>

const Stereo::Replay::ConsistencyChecker::Variant Stereo::Replay::ConsistencyChecker::VARIANT_ARRAY[] =
{
    {Stereo::Replay::EngineDLL::FrameExecution::DIRECT, "as they are",                       false, false},
    {Stereo::Replay::EngineDLL::FrameExecution::DIRECT, "as they are, again",                false, false},
    {Stereo::Replay::EngineDLL::FrameExecution::TWICE,  "twice",                             false, false},
    {Stereo::Replay::EngineDLL::FrameExecution::TWICE,  "twice, the state put back between", true,  false},
    {Stereo::Replay::EngineDLL::FrameExecution::TWICE,  "twice, for the eyes of the recipe", true,  true}
};

/* Says how the executions of a way of replaying differ from the frames as they are. Returns the first frame in which one of the
 * executions differs, and false if there is none.
 */
const Stereo::Replay::ConsistencyChecker::Variant* Stereo::Replay::ConsistencyChecker::get_variant_ptr(const std::string& in_way_name)
{
    const Variant* result_ptr = nullptr;

    if (in_way_name == "again")
    {
        result_ptr = &VARIANT_ARRAY[1];
    }
    else
    if (in_way_name == "twice")
    {
        result_ptr = &VARIANT_ARRAY[2];
    }
    else
    if (in_way_name == "restore")
    {
        result_ptr = &VARIANT_ARRAY[3];
    }
    else
    if (in_way_name == "recipe")
    {
        result_ptr = &VARIANT_ARRAY[4];
    }

    return result_ptr;
}

/* Replays the frames of the options one way, and keeps what the sink is given. */
bool Stereo::Replay::ConsistencyChecker::replay_to_frame(const Stereo::Replay::ConsistencyChecker::Options& in_options,
                                                         const uint32_t&                                    in_frame_index,
                                                         Stereo::Replay::ConsistencyChecker::FrameHashSink* out_sink_ptr,
                                                         std::string*                                       out_error_ptr)
{
    const bool                                  has_deps_file = (in_options.deps_filename.empty  () == false);
    const bool                                  uses_recipe   = (in_options.recipe_filename.empty() == false);
    Stereo::Replay::ConsistencyChecker::Options options       = in_options;

    options.first_frame = in_frame_index;
    options.last_frame  = in_frame_index;

    /* With the history, every frame before the one asked for is replayed with the recipe (it is hurried through, with no image taken), which is what makes
     * what the eyes carry over from frame to frame right; the dependencies leave out what nothing after the frame reads, which makes it quick.
     */
    if (in_options.has_history ||
        has_deps_file)
    {
        options.first_frame = 0;

        out_sink_ptr->set_frames_to_keep({in_frame_index});
    }

    return run_variant(options,
                       VARIANT_ARRAY[(uses_recipe) ? 4 : 0],
                       out_sink_ptr,
                       out_error_ptr);
}

bool Stereo::Replay::ConsistencyChecker::run_variant(const Stereo::Replay::ConsistencyChecker::Options& in_options,
                                                     const Variant&                                     in_variant,
                                                     Stereo::Replay::ConsistencyChecker::FrameHashSink* out_sink_ptr,
                                                     std::string*                                       out_error_ptr,
                                                     ::Replay::ReplayEngineDLL::ICallInterceptor*       in_interceptor_ptr)
{
    constexpr auto POLL_INTERVAL = std::chrono::milliseconds(2);

    Stereo::Replay::EngineDLL::FrameExecutionOptions execution_options;
    bool                                             result            = false;

    auto* const engine_ptr = Stereo::Replay::EngineDLL::aiCreateReplayEngine();

    execution_options.first_frame    = in_options.first_frame;
    execution_options.last_frame     = in_options.last_frame;
    execution_options.mode           = in_variant.mode;
    execution_options.restores_state = in_variant.restores_state;

    {
        auto* const                                      controller_ptr = engine_ptr->get_playback_controller_ptr();
        Stereo::Replay::ConsistencyChecker::StopObserver observer       (controller_ptr,
                                                                         in_options.last_frame);

        engine_ptr->add_replay_observer (&observer);

        if (in_interceptor_ptr != nullptr)
        {
            engine_ptr->add_call_interceptor(in_interceptor_ptr);
        }

        engine_ptr->set_frame_execution (execution_options);
        engine_ptr->set_frame_image_sink(out_sink_ptr);

        if (in_variant.uses_recipe)
        {
            engine_ptr->set_recipe                                       (in_options.recipe);
            engine_ptr->get_playback_controller_ptr()->set_eye_separation(in_options.eye_separation);
        }

        if (engine_ptr->load(in_options.workload_filename.c_str(),
                             out_error_ptr)                         == false)
        {
            goto end;
        }

        const bool has_deps_file = (in_options.deps_filename.empty() == false);

        if (has_deps_file)
        {
            const bool is_deps_set = engine_ptr->set_dependencies_file(in_options.deps_filename.c_str(),
                                                                       out_error_ptr);

            if (is_deps_set == false)
            {
                goto end;
            }
        }

        controller_ptr->set_speed(0.0);

        /* Going to the last frame is what a seek does, which leaves out what the dependencies say nothing after it reads. */
        if (has_deps_file          == false &&
            in_options.has_history == false)
        {
            controller_ptr->play();
        }
        else
        {
            controller_ptr->play_to_frame(in_options.last_frame);
        }

        while (true)
        {
            const auto status = controller_ptr->get_status();

            if (status.state == Stereo::Replay::EngineDLL::PlaybackState::FAILED)
            {
                *out_error_ptr = status.error_message;

                break;
            }

            if (status.state == Stereo::Replay::EngineDLL::PlaybackState::FINISHED ||
                status.state == Stereo::Replay::EngineDLL::PlaybackState::PAUSED)
            {
                result = true;

                break;
            }

            std::this_thread::sleep_for(POLL_INTERVAL);
        }

        if (in_interceptor_ptr != nullptr)
        {
            engine_ptr->remove_call_interceptor(in_interceptor_ptr);
        }

        engine_ptr->remove_replay_observer(&observer);
    }

end:
    Stereo::Replay::EngineDLL::aiDestroyReplayEngine(engine_ptr);

    return result;
}

bool Stereo::Replay::ConsistencyChecker::run_variants(const Stereo::Replay::ConsistencyChecker::Options&                     in_options,
                                                      const std::vector<const Stereo::Replay::ConsistencyChecker::Variant*>& in_variant_ptr_vec,
                                                      std::vector<Stereo::Replay::ConsistencyChecker::FrameHashSink>*        out_sink_vec_ptr,
                                                      std::string*                                                           out_error_ptr)
{
    struct Run
    {
        std::string error;
        bool        is_ok   = false;
        double      seconds = 0.0;
    };

    const size_t     n_variants = in_variant_ptr_vec.size();
    std::vector<Run> run_vec(n_variants);
    bool             result     = true;

    out_sink_vec_ptr->clear();
    out_sink_vec_ptr->reserve(in_variant_ptr_vec.size() );

    for (size_t n_variant = 0;
                n_variant < in_variant_ptr_vec.size();
              ++n_variant)
    {
        out_sink_vec_ptr->emplace_back(false); /* in_keeps_images */
    }

    const auto run_func = [&](const size_t& in_n_variant)
    {
        const auto start_time = std::chrono::steady_clock::now();
        Run&       run        = run_vec.at                    (in_n_variant);

        run.is_ok   = run_variant                  (in_options,
                                                    *in_variant_ptr_vec.at(in_n_variant),
                                                    &out_sink_vec_ptr->at (in_n_variant),
                                                    &run.error);
        run.seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start_time).count();
    };
    const auto print_func = [&](const size_t& in_n_variant)
    {
        fprintf(stdout,
                "%-34s %zu images in %.1f s\n",
                in_variant_ptr_vec.at(in_n_variant)->name_ptr,
                out_sink_vec_ptr->at (in_n_variant).get_hash_map().size(),
                run_vec.at           (in_n_variant).seconds);
        fflush(stdout);
    };

    if (in_options.is_parallel)
    {
        std::vector<std::thread> thread_vec;

        for (size_t n_variant = 0;
                    n_variant < in_variant_ptr_vec.size();
                  ++n_variant)
        {
            thread_vec.emplace_back(run_func,
                                    n_variant);
        }

        for (auto& thread : thread_vec)
        {
            thread.join();
        }
    }

    for (size_t n_variant = 0;
                n_variant < n_variants && result;
              ++n_variant)
    {
        if (in_options.is_parallel == false)
        {
            run_func(n_variant);
        }

        if (run_vec.at(n_variant).is_ok == false)
        {
            *out_error_ptr = std::string("Replaying the frames ")       +
                             in_variant_ptr_vec.at(n_variant)->name_ptr +
                             " failed: "                                +
                             run_vec.at(n_variant).error;

            result = false;
        }
        else
        {
            print_func(n_variant);
        }
    }

    return result;
}
