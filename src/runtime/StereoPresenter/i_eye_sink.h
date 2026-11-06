/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_PRESENTER_I_EYE_SINK_H
#define STEREO_PRESENTER_I_EYE_SINK_H

#include "Stereo/Transport/types.h"

namespace Stereo
{
    namespace Presenter
    {
        /* What shows the images of the eyes that an importer takes: the window which previews them, the headset. A sink takes a frame as
         * it comes, and it is good for no longer than the call: what the sink needs of it, it copies.
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
