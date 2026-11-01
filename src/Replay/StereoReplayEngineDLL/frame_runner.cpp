/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "frame_runner.h"
#include "Common/macros.h"

bool Replay::StereoReplayEngineDLL::FrameRunner::run(const FrameBuffer&                 in_buffer,
                                                     const bool&                        in_is_complete,
                                                     const bool&                        in_skips_repeated_calls,
                                                     const FrameExecutionOptions&       in_options,
                                                     const std::vector<ICallExecutor*>& in_executor_ptr_vec,
                                                     const BeginFunc&                   in_begin_func,
                                                     const GetExecutorFunc&             in_get_executor_func,
                                                     const ExecuteFunc&                 in_execute_func,
                                                     const CaptureFunc&                 in_capture_func)
{
    std::vector<ExecutionClass> class_vec;
    const bool                  is_twice     = (in_is_complete                  &&
                                                in_skips_repeated_calls == false &&
                                                in_options.mode         == FrameExecution::TWICE);
    const size_t                n_calls      = in_buffer.get_call_vec().size();
    const uint32_t              n_executions = (is_twice) ? 2 : 1;
    bool                        result       = false;

    /* Asked once, since the answer is the same in every execution. */
    class_vec.reserve(n_calls);

    for (const auto& call : in_buffer.get_call_vec() )
    {
        class_vec.push_back(in_get_executor_func(call)->get_execution_class(call) );
    }

    if (is_twice                   &&
        in_options.restores_state)
    {
        for (const auto executor_ptr : in_executor_ptr_vec)
        {
            executor_ptr->save_state();
        }
    }

    for (uint32_t n_execution = 0;
                  n_execution < n_executions;
                ++n_execution)
    {
        if (in_begin_func(n_execution) == false)
        {
            goto end;
        }

        if (n_execution               > 0 &&
            in_options.restores_state)
        {
            for (const auto executor_ptr : in_executor_ptr_vec)
            {
                executor_ptr->restore_state();
            }
        }

        for (size_t n_call = 0;
                    n_call < n_calls;
                  ++n_call)
        {
            const auto& call           = in_buffer.get_call_vec().at(n_call);
            bool        should_execute = false;

            switch (class_vec.at(n_call) )
            {
                case ExecutionClass::EVERY_TIME:
                {
                    should_execute = (in_skips_repeated_calls == false);

                    break;
                }

                case ExecutionClass::FIRST_TIME_ONLY:
                {
                    should_execute = (n_execution == 0);

                    break;
                }

                /* What comes after all the executions waits for them, unless the frame is cut short. */
                case ExecutionClass::AFTER_ALL:
                {
                    should_execute = (in_is_complete == false);

                    break;
                }

                default:
                {
                    AI_FAIL_UNSUPPORTED(class_vec.at(n_call) );

                    break;
                }
            }

            if (should_execute)
            {
                if (in_execute_func(call,
                                    in_buffer,
                                    n_call,
                                    n_execution) == false)
                {
                    goto end;
                }
            }
        }

        if (in_is_complete)
        {
            if (in_capture_func(n_execution) == false)
            {
                goto end;
            }
        }
    }

    if (in_is_complete)
    {
        for (size_t n_call = 0;
                    n_call < n_calls;
                  ++n_call)
        {
            const auto& call = in_buffer.get_call_vec().at(n_call);

            if (class_vec.at(n_call) == ExecutionClass::AFTER_ALL)
            {
                if (in_execute_func(call,
                                    in_buffer,
                                    n_call,
                                    n_executions - 1) == false)
                {
                    goto end;
                }
            }
        }
    }

    result = true;

end:
    return result;
}
