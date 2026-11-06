/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * The DLL exports plain C functions (their names are not decorated, whatever the namespace); the interfaces are C++ classes, so the DLL and its users must be built with the
 * same compiler and runtime.
 */
#ifndef STEREO_REPLAY_ENGINEDLL_ENTRYPOINTS_H
#define STEREO_REPLAY_ENGINEDLL_ENTRYPOINTS_H

#include "export.h"
#include "i_replay_engine.h"

namespace Stereo
{
    namespace Replay
    {
        namespace EngineDLL
        {
            extern "C"
            {
                STEREOREPLAYENGINEDLL_API IReplayEngine* aiCreateReplayEngine ();
                STEREOREPLAYENGINEDLL_API void           aiDestroyReplayEngine(IReplayEngine* in_engine_ptr);
            }
        }
    }
}

#endif /* STEREO_REPLAY_ENGINEDLL_ENTRYPOINTS_H */
