/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "replay_profiler.h"
#include <algorithm>
#include <chrono>

Stereo::Replay::ConsistencyChecker::ReplayProfiler::ReplayProfiler(const ClockFunc& in_clock_func)
    :m_clock_func         (in_clock_func),
     m_n_calls            (0),
     m_start_n_nanoseconds(0),
     m_total_n_nanoseconds(0)
{
    if (m_clock_func == nullptr)
    {
        m_clock_func = []()
        {
            return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch() ).count() );
        };
    }
}

std::vector<Stereo::Replay::ConsistencyChecker::ReplayProfiler::FrameEntry> Stereo::Replay::ConsistencyChecker::ReplayProfiler::get_slowest_frames(const size_t& in_n_frames) const
{
    std::vector<FrameEntry> result_vec;

    for (const auto& frame_pair : m_frame_map)
    {
        result_vec.push_back(frame_pair.second);
    }

    std::sort(result_vec.begin(),
              result_vec.end  (),
              [](const FrameEntry& in_frame_a,
                 const FrameEntry& in_frame_b)
              {
                  return (in_frame_a.total_n_nanoseconds != in_frame_b.total_n_nanoseconds) ? (in_frame_a.total_n_nanoseconds > in_frame_b.total_n_nanoseconds)
                                                                                            : (in_frame_a.n_frame                < in_frame_b.n_frame);
              });

    if (result_vec.size() > in_n_frames)
    {
        result_vec.resize(in_n_frames);
    }

    return result_vec;
}

std::vector<Stereo::Replay::ConsistencyChecker::ReplayProfiler::FunctionEntry> Stereo::Replay::ConsistencyChecker::ReplayProfiler::get_slowest_functions(const size_t& in_n_functions) const
{
    std::vector<FunctionEntry> result_vec;

    for (const auto& function_pair : m_function_map)
    {
        result_vec.push_back(function_pair.second);
    }

    std::sort(result_vec.begin(),
              result_vec.end  (),
              [](const FunctionEntry& in_function_a,
                 const FunctionEntry& in_function_b)
              {
                  return (in_function_a.total_n_nanoseconds != in_function_b.total_n_nanoseconds) ? (in_function_a.total_n_nanoseconds > in_function_b.total_n_nanoseconds)
                                                                                                  : (in_function_a.function            < in_function_b.function);
              });

    if (result_vec.size() > in_n_functions)
    {
        result_vec.resize(in_n_functions);
    }

    return result_vec;
}

uint64_t Stereo::Replay::ConsistencyChecker::ReplayProfiler::get_n_calls() const
{
    return m_n_calls;
}

uint64_t Stereo::Replay::ConsistencyChecker::ReplayProfiler::get_total_n_nanoseconds() const
{
    return m_total_n_nanoseconds;
}

void Stereo::Replay::ConsistencyChecker::ReplayProfiler::on_call_replayed(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                          const uint32_t&                              in_n_frame,
                                                                          const bool&                                  in_was_executed)
{
    const uint64_t n_nanoseconds = (in_was_executed) ? (m_clock_func() - m_start_n_nanoseconds)
                                                     : 0;

    if (in_was_executed)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        FrameEntry&    frame_entry    = m_frame_map[in_n_frame];
        FunctionEntry& function_entry = m_function_map[in_call.func];

        frame_entry.n_frame              = in_n_frame;
        frame_entry.n_calls++;
        frame_entry.total_n_nanoseconds += n_nanoseconds;

        if (n_nanoseconds > frame_entry.slowest_n_nanoseconds)
        {
            frame_entry.slowest_function      = in_call.func;
            frame_entry.slowest_n_nanoseconds = n_nanoseconds;
        }

        function_entry.function            = in_call.func;
        function_entry.max_n_nanoseconds   = std::max(function_entry.max_n_nanoseconds,
                                                      n_nanoseconds);
        function_entry.n_calls++;
        function_entry.total_n_nanoseconds += n_nanoseconds;

        m_n_calls++;
        m_total_n_nanoseconds += n_nanoseconds;
    }
}

bool Stereo::Replay::ConsistencyChecker::ReplayProfiler::should_execute_call(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                             const uint32_t&                              in_n_frame)
{
    (void) in_call;
    (void) in_n_frame;

    m_start_n_nanoseconds = m_clock_func();

    return true;
}
