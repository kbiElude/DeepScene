/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_REPLAY_ENGINEDLL_NULL_REPLAY_OBSERVER_H
#define STEREO_REPLAY_ENGINEDLL_NULL_REPLAY_OBSERVER_H

#include "tools/stereo_replay/engine_dll/i_replay_observer.h"

namespace Stereo
{
    namespace Replay
    {
        namespace EngineDLL
        {
            /* A replay observer that ignores every notification. An observer that needs only some of them derives from this class and
             * overrides those. The class is implemented entirely in the header because the tools that use it load the engine as a
             * library at run time.
             */
            class NullReplayObserver : public IReplayObserver
            {
            public:
                void on_error(const std::string& in_message) override
                {
                    (void) in_message;
                }

                void on_frame_presented(const uint32_t& in_n_frame) override
                {
                    (void) in_n_frame;
                }

                void on_state_changed(const PlaybackState& in_old_state,
                                      const PlaybackState& in_new_state) override
                {
                    (void) in_old_state;
                    (void) in_new_state;
                }

                void on_window_closed() override
                {
                    /* Stub */
                }

                void on_window_placed(const WindowPlacement& in_placement) override
                {
                    (void) in_placement;
                }
            };
        }
    }
}

#endif /* STEREO_REPLAY_ENGINEDLL_NULL_REPLAY_OBSERVER_H */
