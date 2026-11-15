/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Records which call a replay was making when it crashed. The call that is about to be made is written as text to a file that is mapped
 * into the memory of the process. Data written to a mapped file belongs to the operating system and stays in the file when the process
 * dies, so nothing has to be flushed and a crash loses nothing. The file holds one line: whether the call was begun or finished, the frame,
 * the number of calls the replay has made, the thread, and the call with its arguments (pointers in hexadecimal, data of the workload as the
 * number of its chunk).
 */
#ifndef STEREO_REPLAY_INSPECTOR_CRASH_BREADCRUMB_H
#define STEREO_REPLAY_INSPECTOR_CRASH_BREADCRUMB_H

#include "replay/replay_engine_dll/i_call_interceptor.h"
#include <stdint.h>
#include <string>

namespace Stereo
{
    namespace Replay
    {
        namespace Inspector
        {
            class CrashBreadcrumb : public ::Replay::ReplayEngineDLL::ICallInterceptor
            {
            public:
                /* Creates the file, or empties it, and sizes it for the longest line. If the file cannot be created, the breadcrumb does nothing. */
                explicit CrashBreadcrumb(const std::string& in_filename);
                ~CrashBreadcrumb();

                bool is_open() const;

                void on_call_replayed(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                      const uint32_t&                              in_n_frame,
                                      const bool&                                  in_was_executed) override;

                bool should_execute_call(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                         const uint32_t&                              in_n_frame) override;


                /* Formats a call as one line of text: the state ("begun" or "finished"), the frame, the call number, the thread, the function and its
                 * arguments. */
                static std::string format_call(const std::string&                           in_state,
                                               const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                               const uint32_t&                              in_n_frame,
                                               const uint64_t&                              in_n_call);

            private:
                /* The maximum length of a line in bytes. The rest of a longer argument list is cut off. */
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

#endif /* STEREO_REPLAY_INSPECTOR_CRASH_BREADCRUMB_H */
