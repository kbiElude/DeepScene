/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Replays a workload in the ways that the inspector compares: the frames as they are, executed twice, and rendered for the eyes with a recipe.
 */
#ifndef STEREO_REPLAY_INSPECTOR_REPLAY_RUNNER_H
#define STEREO_REPLAY_INSPECTOR_REPLAY_RUNNER_H

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
        namespace Inspector
        {
            extern const ReplayWay REPLAY_WAY_ARRAY[6];

            /* The way of replaying that the name stands for (see print_usage), or nullptr. */
            const ReplayWay* get_replay_way_ptr(const std::string& in_way_name);

            /* Replays the workload up to the frame (including the frames before it, if the options ask for history), with the recipe of the options
             * if there is one, so that the sink has the image of the frame. The frame is the only one that the sink keeps.
             */
            bool replay_to_frame(const Options&  in_options,
                                 const uint32_t& in_frame_index,
                                 FrameHashSink*  out_sink_ptr,
                                 std::string*    out_error_ptr);

            /* Replays the workload up to the frame as replay_to_frame() does, using the recipe and the dependencies of the options (it needs both).
             * The frame itself is played once per variant, for the left eye, each time from the state that the frames before it leave. It returns
             * the image of each variant, in the order of the variants. It returns false, with the reason, if the frame cannot be played this way (for
             * example because it carries the back buffer over); the variants are then replayed one at a time.
             */
            bool replay_variants_to_frame(const Options&                                            in_options,
                                          const uint32_t&                                           in_frame_index,
                                          const std::vector<Stereo::Replay::EngineDLL::FrameVariant>& in_variant_vec,
                                          std::vector<Utils::Images::Image>*                        out_image_vec_ptr,
                                          std::string*                                              out_error_ptr);

            /* Replays the frames of the options one way, and keeps what the sink is given. */
            bool run_variant(const Options&                               in_options,
                             const ReplayWay&                             in_replay_way,
                             FrameHashSink*                               out_sink_ptr,
                             std::string*                                 out_error_ptr,
                             ::Replay::ReplayEngineDLL::ICallInterceptor* in_interceptor_ptr = nullptr);

            /* Replays the frames once per way in the vector, one after the other or all at once, and prints how long each took. The sinks are in the
             * order of the ways.
             */
            bool run_variants(const Options&                     in_options,
                              const std::vector<const ReplayWay*>& in_replay_way_ptr_vec,
                              std::vector<FrameHashSink>*        out_sink_vec_ptr,
                              std::string*                       out_error_ptr);
        }
    }
}

#endif /* STEREO_REPLAY_INSPECTOR_REPLAY_RUNNER_H */
