/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "frame_runner.h"
#include "common/macros.h"
#include <algorithm>

bool Stereo::Replay::EngineDLL::FrameRunner::run(const FrameBuffer&                                            in_buffer,
                                                 const bool&                                                   in_is_complete,
                                                 const bool&                                                   in_skips_repeated_calls,
                                                 const FrameExecutionOptions&                                  in_options,
                                                 const std::vector<::Replay::ReplayEngineDLL::ICallExecutor*>& in_executor_ptr_vec,
                                                 const BeginFunc&                                              in_begin_func,
                                                 const GetExecutorFunc&                                        in_get_executor_func,
                                                 const ExecuteFunc&                                            in_execute_func,
                                                 const CaptureFunc&                                            in_capture_func)
{
    std::vector<::Replay::ReplayEngineDLL::ExecutionClass> class_vec;

    /* True if the frame is executed more than once: twice (one execution per eye) or once per variant. */
    const bool     is_repeated  = (in_is_complete                                    &&
                                   in_skips_repeated_calls == false                  &&
                                   (in_options.mode        == FrameExecution::TWICE  ||
                                    in_options.mode        == FrameExecution::VARIANTS) );
    const size_t   n_calls      = in_buffer.get_call_vec().size();
    const uint32_t n_executions = (is_repeated) ? ( (in_options.mode == FrameExecution::TWICE) ? 2
                                                                                                : std::max<uint32_t>(1,
                                                                                                                     static_cast<uint32_t>(in_options.variant_vec.size() ) ) )
                                                : 1;
    bool           result       = false;

    /* The execution class of a call is the same in every execution, so it is looked up once. */
    class_vec.reserve(n_calls);

    for (const auto& call : in_buffer.get_call_vec() )
    {
        class_vec.push_back(in_get_executor_func(call)->get_execution_class(call) );
    }

    if (is_repeated                &&
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

            /* A restore consumes the saved state. Save it again unless this is the last execution, so that every execution starts
             * from the same state as the first one.
             */
            if (n_execution + 1 < n_executions)
            {
                for (const auto executor_ptr : in_executor_ptr_vec)
                {
                    executor_ptr->save_state();
                }
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
                case ::Replay::ReplayEngineDLL::ExecutionClass::EVERY_TIME:
                {
                    should_execute = (in_skips_repeated_calls == false);

                    break;
                }

                case ::Replay::ReplayEngineDLL::ExecutionClass::FIRST_TIME_ONLY:
                {
                    should_execute = (n_execution == 0);

                    break;
                }

                /* These calls run after the last execution (see the loop below). An incomplete frame has a single execution, so its
                 * calls of this class run here.
                 */
                case ::Replay::ReplayEngineDLL::ExecutionClass::AFTER_ALL:
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

            if (class_vec.at(n_call) == ::Replay::ReplayEngineDLL::ExecutionClass::AFTER_ALL)
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
