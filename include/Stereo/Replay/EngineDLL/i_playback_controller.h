/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_REPLAY_ENGINEDLL_I_PLAYBACK_CONTROLLER_H
#define STEREO_REPLAY_ENGINEDLL_I_PLAYBACK_CONTROLLER_H

#include "Replay/ReplayEngineDLL/i_playback_controller.h"
#include "Stereo/Replay/EngineDLL/types.h"

namespace Stereo
{
    namespace Replay
    {
        namespace EngineDLL
        {
            /* Controls a replay from any thread (see the controller of the replay engine of the interceptor for play, pause, reset and the
             * status), and says how far apart the eyes are and how fast the replay goes.
             */
            class IPlaybackController : public ::Replay::ReplayEngineDLL::IPlaybackController
            {
            public:
                /* The distance between the eyes, in the units of the camera of the application. */
                virtual float get_eye_separation() const = 0;

                /* Changes the distance between the eyes of the frames that are rendered for two eyes; 0 renders the same image for both.
                 * Takes effect from the next frame, and is not part of any recipe: a recipe says in which direction the eyes are moved
                 * apart (an eye offset), and this says how far. It starts as 0.
                 */
                virtual void set_eye_separation(const float& in_eye_separation) = 0;

                /* The pace of the replay: 1 is the pace at which the dumped process presented its frames, 2 is twice as fast, 0 is as
                 * fast as the machine can go. Takes effect at once.
                 */
                virtual void set_speed(const double& in_speed) = 0;
            };
        }
    }
}

#endif /* STEREO_REPLAY_ENGINEDLL_I_PLAYBACK_CONTROLLER_H */
