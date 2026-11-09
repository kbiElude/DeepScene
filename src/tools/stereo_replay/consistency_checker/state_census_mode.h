/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Says what the calls of the dead frames of a seek do to the state of the context.
 */
#ifndef STEREO_REPLAY_CONSISTENCYCHECKER_STATE_CENSUS_MODE_H
#define STEREO_REPLAY_CONSISTENCYCHECKER_STATE_CENSUS_MODE_H

#include "types.h"
#include <string>

namespace Stereo
{
    namespace Replay
    {
        namespace ConsistencyChecker
        {
            /* Replays the frames of the options once, the way a seek to the last of them does (the dependencies of the options say which frames are not made),
             * and says, of the calls of OpenGL in the frames that are not made, how many give a piece of state the value it has and how many are overwritten
             * before anything that is made looks at them (see StateCensus).
             */
            bool take_state_census(const Options& in_options,
                                   std::string*   out_error_ptr);
        }
    }
}

#endif /* STEREO_REPLAY_CONSISTENCYCHECKER_STATE_CENSUS_MODE_H */
