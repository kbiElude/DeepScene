/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_REPLAY_ENGINEDLL_I_FRAME_IMAGE_SINK_H
#define STEREO_REPLAY_ENGINEDLL_I_FRAME_IMAGE_SINK_H

#include "types.h"

namespace Stereo
{
    namespace Replay
    {
        namespace EngineDLL
        {
            /* Receives the back buffer after each execution of a frame (see FrameExecutionOptions), before the frame is presented.
             * Called from the replay thread.
             */
            class IFrameImageSink
            {
            public:
                virtual ~IFrameImageSink() = default;

                /* The executions of a frame are numbered from 0. */
                virtual void on_frame_image(const uint32_t&   in_n_frame,
                                            const uint32_t&   in_n_execution,
                                            const FrameImage& in_image) = 0;

                /* Whether the sink wants the image. Reading the image makes the replay wait for the device. Called from the replay
                 * thread before each execution is read. A frame that is wanted in one execution should be wanted in all of them.
                 */
                virtual bool wants_frame_image(const uint32_t& in_n_frame,
                                               const uint32_t& in_n_execution) const = 0;
            };
        }
    }
}

#endif /* STEREO_REPLAY_ENGINEDLL_I_FRAME_IMAGE_SINK_H */
