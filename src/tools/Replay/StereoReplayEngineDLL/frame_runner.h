/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_REPLAY_ENGINEDLL_FRAME_RUNNER_H
#define STEREO_REPLAY_ENGINEDLL_FRAME_RUNNER_H

#include "types.h"
#include "frame_buffer.h"
#include "i_call_executor.h"
#include <functional>
#include <vector>

namespace Stereo
{
    namespace Replay
    {
        namespace EngineDLL
        {
            /* Executes the calls of a buffered frame once or twice. A call is executed when its executor says (see
             * ExecutionClass): state, draws and uploads in every execution, what creates objects in the first one, and what destroys
             * objects, and the present, after the last one. The back buffer is handed out after each execution, and each one is told when it begins.
             */
            class FrameRunner
            {
            public:
                typedef std::function<bool(const uint32_t& in_n_execution)> BeginFunc;
                typedef std::function<bool(const uint32_t& in_n_execution)> CaptureFunc;

                /* The call is the n-th of the frame, and is executed in the given execution. */
                typedef std::function<bool          (const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                     const ::Replay::ReplayEngineDLL::IChunkSource&                          in_chunk_source,
                                                     const size_t&                                in_n_call,
                                                     const uint32_t&                              in_n_execution)> ExecuteFunc;
                typedef std::function<::Replay::ReplayEngineDLL::ICallExecutor*(const Capture::APIDumpLoader::DumpedAPICall& in_call)>        GetExecutorFunc;

                /* The frame is complete if it ends with the present; otherwise (the workload ends in the middle of it) it is executed
                 * once whatever the options say, and nothing is captured. A frame which skips the calls that repeat is executed once without
                 * them: what creates and destroys objects still goes on, so that the frames after it find what they need. Returns false if a
                 * call fails, or the begin or capture function does.
                 */
                static bool run(const FrameBuffer&                                            in_buffer,
                                const bool&                                                   in_is_complete,
                                const bool&                                                   in_skips_repeated_calls,
                                const FrameExecutionOptions&                                  in_options,
                                const std::vector<::Replay::ReplayEngineDLL::ICallExecutor*>& in_executor_ptr_vec,
                                const BeginFunc&                                              in_begin_func,
                                const GetExecutorFunc&                                        in_get_executor_func,
                                const ExecuteFunc&                                            in_execute_func,
                                const CaptureFunc&                                            in_capture_func);
            };
        }
    }
}

#endif /* STEREO_REPLAY_ENGINEDLL_FRAME_RUNNER_H */
