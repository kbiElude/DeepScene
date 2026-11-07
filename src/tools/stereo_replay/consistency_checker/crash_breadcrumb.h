/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Says which call a replay was making when it crashed: the call about to be made is written, as text, to a file that is mapped into the memory of
 * the process. What a program has written to a mapped file is the system's, and stays in the file when the program dies, so there is nothing to
 * flush and nothing to be lost to a crash. The file is one line: whether the call was begun or finished, the frame it is in, how many calls the replay
 * has made, the thread, and the call with the numbers of its arguments (pointers in hexadecimal, data of the workload as the number of its chunk).
 */
#ifndef STEREO_REPLAY_CONSISTENCYCHECKER_CRASH_BREADCRUMB_H
#define STEREO_REPLAY_CONSISTENCYCHECKER_CRASH_BREADCRUMB_H

#include "replay/replay_engine_dll/i_call_interceptor.h"
#include <stdint.h>
#include <string>

namespace Stereo
{
    namespace Replay
    {
        namespace ConsistencyChecker
        {
            class CrashBreadcrumb : public ::Replay::ReplayEngineDLL::ICallInterceptor
            {
            public:
                /* The file is made, or made empty, and is as long as one line can be. If it cannot be made, the breadcrumb does nothing. */
                explicit CrashBreadcrumb(const std::string& in_filename);
                ~CrashBreadcrumb();

                /* The call as a line of text: the state ("begun" or "finished"), the frame, the number of the call, the thread, the function and its arguments. */
                static std::string format_call(const std::string&                           in_state,
                                               const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                               const uint32_t&                              in_n_frame,
                                               const uint64_t&                              in_n_call);

                bool is_open() const;

                void on_call_replayed(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                      const uint32_t&                              in_n_frame,
                                      const bool&                                  in_was_executed) override;

                bool should_execute_call(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                         const uint32_t&                              in_n_frame) override;

            private:
                /* The most that a line is (the rest of an argument list is left out). */
                static constexpr size_t LINE_N_BYTES = 1024;

                void write_line(const std::string& in_line);

                void*    m_file_handle;
                void*    m_mapping_handle;
                uint64_t m_n_calls;
                char*    m_view_ptr;
            };
        }
    }
}

#endif /* STEREO_REPLAY_CONSISTENCYCHECKER_CRASH_BREADCRUMB_H */
