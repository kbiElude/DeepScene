/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_REPLAY_ENGINEDLL_NULL_CALL_EXECUTOR_H
#define STEREO_REPLAY_ENGINEDLL_NULL_CALL_EXECUTOR_H

#include "i_call_executor.h"

namespace Stereo
{
    namespace Replay
    {
        namespace EngineDLL
        {
            /* Accepts every call and does nothing with it: stands in for the APIs which have no executor yet. */
            class NullCallExecutor : public ICallExecutor
            {
            public:
                NullCallExecutor();

                bool execute           (const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                        const IChunkSource&                          in_chunk_source,
                                        std::string*                                 out_error_ptr) override;
                void on_frame_presented() override;
                void reset             () override;
            };
        }
    }
}

#endif /* STEREO_REPLAY_ENGINEDLL_NULL_CALL_EXECUTOR_H */
