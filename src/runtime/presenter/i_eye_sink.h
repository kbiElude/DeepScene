/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_PRESENTER_I_EYE_SINK_H
#define STEREO_PRESENTER_I_EYE_SINK_H

#include "stereo/transport/types.h"

namespace Stereo
{
    namespace Presenter
    {
        /* Shows the eye images that an importer provides, for example the preview window or the headset. A sink receives each frame as it arrives.
         * The frame is valid only for the duration of the call, so the sink copies whatever it needs.
         */
        class IEyeSink
        {
        public:
            virtual ~IEyeSink() = default;

            virtual void on_frame(const Stereo::Transport::ImportedFrame& in_frame) = 0;
        };
    }
}

#endif /* STEREO_PRESENTER_I_EYE_SINK_H */
