/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Shows where the time of a replay goes: the time that each call takes, summed by function and by frame. It observes the calls through the
 * call interception of the engine. The time between the engine asking whether to execute a call and reporting that it was replayed is the
 * time of the call; a skipped call has none. Frames are the ones that the engine counts. The time of a frame is the time of its calls,
 * without any waiting for the pace of the dumped program.
 */
#ifndef STEREO_REPLAY_INSPECTOR_REPLAY_PROFILER_H
#define STEREO_REPLAY_INSPECTOR_REPLAY_PROFILER_H

#include "replay/replay_engine_dll/i_call_interceptor.h"
#include <functional>
#include <map>
#include <mutex>
#include <stdint.h>
#include <vector>

namespace Stereo
{
    namespace Replay
    {
        namespace Inspector
        {
            class ReplayProfiler : public ::Replay::ReplayEngineDLL::ICallInterceptor
            {
            public:
                /* A timestamp in nanoseconds. Only differences between timestamps are meaningful. */
                typedef std::function<uint64_t()> ClockFunc;

                /* The time spent in the calls of one function. */
                struct FunctionEntry
                {
                    APIInterceptor::APIFunction function            = APIInterceptor::APIFUNCTION_UNKNOWN;
                    uint64_t                    max_n_nanoseconds   = 0; /* the longest single call */
                    uint64_t                    n_calls             = 0;
                    uint64_t                    total_n_nanoseconds = 0;
                };

                /* The time spent in the calls of one frame. */
                struct FrameEntry
                {
                    uint32_t                    n_frame                 = 0;
                    uint64_t                    n_calls                 = 0;
                    APIInterceptor::APIFunction slowest_function        = APIInterceptor::APIFUNCTION_UNKNOWN;
                    uint64_t                    slowest_n_nanoseconds   = 0; /* the longest single call */
                    uint64_t                    total_n_nanoseconds     = 0;
                };

                /* The default clock is the steady clock of the system; another clock can be passed in. */
                explicit ReplayProfiler(const ClockFunc& in_clock_func = ClockFunc());

                /* Read the results only after the replay is over. The thread of the engine writes them during the replay. */

                /* The frames that took longest, longest first. */
                std::vector<FrameEntry> get_slowest_frames(const size_t& in_n_frames) const;

                /* The functions with the largest total call time, longest first. */
                std::vector<FunctionEntry> get_slowest_functions(const size_t& in_n_functions) const;

                uint64_t get_n_calls            () const;
                uint64_t get_total_n_nanoseconds() const;

                void on_call_replayed(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                      const uint32_t&                              in_n_frame,
                                      const bool&                                  in_was_executed) override;

                bool should_execute_call(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                         const uint32_t&                              in_n_frame) override;

            private:
                ClockFunc                                            m_clock_func;
                std::map<uint32_t, FrameEntry>                       m_frame_map;
                std::map<APIInterceptor::APIFunction, FunctionEntry> m_function_map;
                std::mutex                                           m_mutex;
                uint64_t                                             m_n_calls;
                uint64_t                                             m_start_n_nanoseconds;
                uint64_t                                             m_total_n_nanoseconds;
            };
        }
    }
}

#endif /* STEREO_REPLAY_INSPECTOR_REPLAY_PROFILER_H */
