/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "presenter_log.h"
#include <Windows.h>

Stereo::Presenter::PresenterLog::State& Stereo::Presenter::PresenterLog::get_state()
{
    static State state;

    return state;
}

void Stereo::Presenter::PresenterLog::write_line(const std::string& in_path,
                                                 const std::string& in_text)
{
    State&                      state = get_state();
    std::lock_guard<std::mutex> lock(state.mutex);

    if (state.path != in_path)
    {
        if (state.file != INVALID_HANDLE_VALUE)
        {
            ::CloseHandle(state.file);
        }

        state.file = ::CreateFileA(in_path.c_str(),
                                   FILE_APPEND_DATA,
                                   FILE_SHARE_READ | FILE_SHARE_WRITE,
                                   nullptr,
                                   OPEN_ALWAYS,
                                   FILE_ATTRIBUTE_NORMAL,
                                   nullptr);
        state.path = in_path;
    }

    if (state.file != INVALID_HANDLE_VALUE)
    {
        DWORD             n_written = 0;
        const std::string line      = in_text + " (presenter)\r\n";

        ::WriteFile(state.file,
                    line.c_str(),
                    static_cast<DWORD>(line.size() ),
                   &n_written,
                    nullptr);
    }
}
