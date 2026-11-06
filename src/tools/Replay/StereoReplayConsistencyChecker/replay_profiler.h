/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Where the time of a replay goes: the time that each call takes, summed by function and by frame. It sees the calls through the interception of the
 * engine: the time between the engine asking whether to execute a call and its saying that it was replayed is the call's (a call that is skipped has
 * none). Frames are the ones that the engine counts, and a frame's time is that of its calls, without any waiting for the pace of the dumped program.
 */
#ifndef STEREO_REPLAY_CONSISTENCYCHECKER_REPLAY_PROFILER_H
#define STEREO_REPLAY_CONSISTENCYCHECKER_REPLAY_PROFILER_H

#include "Replay/ReplayEngineDLL/i_call_interceptor.h"
#include <functional>
#include <map>
#include <mutex>
#include <stdint.h>
#include <vector>

namespace Stereo
{
    namespace Replay
    {
        namespace ConsistencyChecker
        {
            class ReplayProfiler : public ::Replay::ReplayEngineDLL::ICallInterceptor
            {
            public:
                /* The time in nanoseconds, from any starting point. */
                typedef std::function<uint64_t()> ClockFunc;

                /* What the calls of one function took. */
                struct FunctionEntry
                {
                    APIInterceptor::APIFunction function            = APIInterceptor::APIFUNCTION_UNKNOWN;
                    uint64_t                    max_n_nanoseconds   = 0; /* of a call */
                    uint64_t                    n_calls             = 0;
                    uint64_t                    total_n_nanoseconds = 0;
                };

                /* What the calls of one frame took. */
                struct FrameEntry
                {
                    uint32_t                    n_frame                 = 0;
                    uint64_t                    n_calls                 = 0;
                    APIInterceptor::APIFunction slowest_function        = APIInterceptor::APIFUNCTION_UNKNOWN;
                    uint64_t                    slowest_n_nanoseconds   = 0; /* a call */
                    uint64_t                    total_n_nanoseconds     = 0;
                };

                /* The clock is the steady clock of the system unless another is given. */
                explicit ReplayProfiler(const ClockFunc& in_clock_func = ClockFunc() );

                /* What was seen is read when the replay is over: the engine's thread is the one that writes it. */

                /* The frames that took longest, longest first. */
                std::vector<FrameEntry> get_slowest_frames(const size_t& in_n_frames) const;

                /* The functions whose calls took longest in all, longest first. */
                std::vector<FunctionEntry> get_slowest_functions(const size_t& in_n_functions) const;

                uint64_t get_n_calls() const;
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

#endif /* STEREO_REPLAY_CONSISTENCYCHECKER_REPLAY_PROFILER_H */
