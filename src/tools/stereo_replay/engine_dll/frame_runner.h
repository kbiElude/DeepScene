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
            /* Executes the calls of a buffered frame once or more (twice for the two eyes, or once per variant). The executor of a
             * call chooses when it runs through its ExecutionClass. State changes, draws and uploads run in every execution, calls that
             * create objects run in the first execution, and calls that destroy objects and the present run after the last one. The
             * begin function is called before each execution and the capture function after it, to hand out the back buffer.
             */
            class FrameRunner
            {
            public:
                typedef std::function<bool(const uint32_t& in_n_execution)> BeginFunc;
                typedef std::function<bool(const uint32_t& in_n_execution)> CaptureFunc;

                /* Executes in_call, the in_n_call-th call of the frame, as part of the execution in_n_execution. */
                typedef std::function<bool                                     (const Capture::APIDumpLoader::DumpedAPICall&   in_call,
                                                                                const ::Replay::ReplayEngineDLL::IChunkSource& in_chunk_source,
                                                                                const size_t&                                  in_n_call,
                                                                                const uint32_t&                                in_n_execution)> ExecuteFunc;
                typedef std::function<::Replay::ReplayEngineDLL::ICallExecutor*(const Capture::APIDumpLoader::DumpedAPICall&   in_call)>        GetExecutorFunc;

                /* A frame is complete if it ends with the present. An incomplete frame (the workload ends in the middle of it) is
                 * executed once whatever the options say, and nothing is captured. If in_skips_repeated_calls is set, the frame is executed
                 * once without the calls that repeat. Calls that create or destroy objects still run, so that later frames find the
                 * objects they need. Returns false if a call fails, or if the begin or capture function does.
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
