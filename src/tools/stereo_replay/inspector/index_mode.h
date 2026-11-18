/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Makes the index of a workload, so that the seeks that follow find it.
 */
#ifndef STEREO_REPLAY_INSPECTOR_INDEX_MODE_H
#define STEREO_REPLAY_INSPECTOR_INDEX_MODE_H

#include "types.h"
#include <string>

namespace Stereo
{
    namespace Replay
    {
        namespace Inspector
        {
            /* Makes the index of the workload (one read of all of it) and keeps it in a file next to it, unless there is one of the workload as it is, and says
             * what it holds. The seeks (--screenshot with --deps, the replay player) use the index to read only the segments they need, and the state frames
             * of the dead ones; without it every segment is read in full.
             */
            bool make_index(const Options& in_options,
                            std::string*   out_error_ptr);
        }
    }
}

#endif /* STEREO_REPLAY_INSPECTOR_INDEX_MODE_H */
