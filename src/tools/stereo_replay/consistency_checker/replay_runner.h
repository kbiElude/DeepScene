/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Replays a workload the ways which the checker compares: the frames as they are, executed twice, and for the eyes of a recipe.
 */
#ifndef STEREO_REPLAY_CONSISTENCYCHECKER_REPLAY_RUNNER_H
#define STEREO_REPLAY_CONSISTENCYCHECKER_REPLAY_RUNNER_H

#include "frame_hash_sink.h"
#include "replay/replay_engine_dll/i_call_interceptor.h"
#include "types.h"
#include <stdint.h>
#include <string>
#include <vector>

namespace Stereo
{
    namespace Replay
    {
        namespace ConsistencyChecker
        {
            extern const Variant VARIANT_ARRAY[5];

            /* The way of replaying that the name stands for (see print_usage), or nullptr. */
            const Variant* get_variant_ptr(const std::string& in_way_name);

            /* Replays the workload up to the frame (with the frames before it, if the options say so), with the recipe of the options if they have one, so that
             * the sink has the image of the frame. The frame is the only one the sink keeps.
             */
            bool replay_to_frame(const Options&  in_options,
                                 const uint32_t& in_frame_index,
                                 FrameHashSink*  out_sink_ptr,
                                 std::string*    out_error_ptr);

            /* Replays the frames of the options one way, and keeps what the sink is given. */
            bool run_variant(const Options&                               in_options,
                             const Variant&                               in_variant,
                             FrameHashSink*                               out_sink_ptr,
                             std::string*                                 out_error_ptr,
                             ::Replay::ReplayEngineDLL::ICallInterceptor* in_interceptor_ptr = nullptr);

            /* Replays the frames the ways in the vector, one after the other or all at once, and prints how long each took. The sinks are in the order of
             * the ways.
             */
            bool run_variants(const Options&                     in_options,
                              const std::vector<const Variant*>& in_variant_ptr_vec,
                              std::vector<FrameHashSink>*        out_sink_vec_ptr,
                              std::string*                       out_error_ptr);
        }
    }
}

#endif /* STEREO_REPLAY_CONSISTENCYCHECKER_REPLAY_RUNNER_H */
