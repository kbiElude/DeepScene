/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Says what the calls of the dead frames of a seek do to the state of the context.
 */
#ifndef STEREO_REPLAY_INSPECTOR_REDUNDANT_STATE_COUNTER_MODE_H
#define STEREO_REPLAY_INSPECTOR_REDUNDANT_STATE_COUNTER_MODE_H

#include "types.h"
#include <string>

namespace Stereo
{
    namespace Replay
    {
        namespace Inspector
        {
            /* Replays the frames of the options once, the way a seek to the last of them does (the dependencies of the options say which frames are
             * not made). For the OpenGL calls in the frames that are not made, it reports how many set a piece of state to the value that it already
             * has and how many are overwritten before anything that is made looks at them (see RedundantStateCounter).
             */
            bool take_redundant_state_count(const Options& in_options,
                                   std::string*   out_error_ptr);
        }
    }
}

#endif /* STEREO_REPLAY_INSPECTOR_REDUNDANT_STATE_COUNTER_MODE_H */
