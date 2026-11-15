/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Says where the time of a replay goes.
 */
#ifndef STEREO_REPLAY_INSPECTOR_PROFILE_MODE_H
#define STEREO_REPLAY_INSPECTOR_PROFILE_MODE_H

#include "types.h"
#include <string>

namespace Stereo
{
    namespace Replay
    {
        namespace Inspector
        {
            /* Replays the frames of the options once (with the recipe, if there is one) and reports where the call time went: the functions that took
             * longest, and the frames.
             */
            bool take_profile(const Options& in_options,
                              std::string*   out_error_ptr);
        }
    }
}

#endif /* STEREO_REPLAY_INSPECTOR_PROFILE_MODE_H */
