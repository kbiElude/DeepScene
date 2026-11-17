/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_REPLAY_ENGINEDLL_I_SLICE_CALL_HANDLER_H
#define STEREO_REPLAY_ENGINEDLL_I_SLICE_CALL_HANDLER_H

#include "capture/api_dump_loader/api_dump_loader.h"
#include "capture/api_dump_loader/types.h"
#include "types.h"

namespace Stereo
{
    namespace Replay
    {
        namespace EngineDLL
        {
            /* What the frames that a seek leaves out (dead frames) do to the calls of one API. SliceSkipper hands each call to the handler of
             * its API, so what only one API needs is not done for the others.
             */
            class ISliceCallHandler
            {
            public:
                virtual ~ISliceCallHandler() = default;

                /* Makes the function that the reader asks for each call of one segment (see CallKeptFunc). Empty: every call of the API is kept. */
                virtual Capture::APIDumpLoader::CallKeptFunc create_call_kept_func(const FrameLiveFunc& in_is_live_func) const = 0;

                virtual Capture::APIDumpLoader::APIGroup get_group() const = 0;

                /* Whether the reader reads the data chunks of the call when its frame is dead. */
                virtual bool is_chunk_wanted_in_dead_frame(const Capture::APIDumpLoader::DumpedAPICall& in_call) const = 0;

                /* Called for every call of the API, in order. Returns whether the call is executed. */
                virtual bool should_execute_call(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                 const bool&                                  in_is_dead_frame) = 0;
            };
        }
    }
}

#endif /* STEREO_REPLAY_ENGINEDLL_I_SLICE_CALL_HANDLER_H */
