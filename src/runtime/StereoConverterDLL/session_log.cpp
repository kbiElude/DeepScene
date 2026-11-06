/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "Stereo/Session/session_environment.h"
#include "session_log.h"
#include <Windows.h>

Stereo::ConverterDLL::SessionLog::SessionLog(const Stereo::Session::SessionInfo& in_session_info)
    :m_file       (INVALID_HANDLE_VALUE),
     m_is_enabled (in_session_info.is_logging),
     m_log_path   (Stereo::Session::SessionEnvironment::get_log_path(in_session_info.session_id) ),
     m_recipe_path(in_session_info.recipe_path)
{
    char program_i8vec260[MAX_PATH] = {};

    ::GetModuleFileNameA(nullptr,
                         program_i8vec260,
                         MAX_PATH);

    m_program_path = program_i8vec260;
}

Stereo::ConverterDLL::SessionLog::~SessionLog()
{
    if (m_file != INVALID_HANDLE_VALUE)
    {
        ::CloseHandle(m_file);
    }
}

bool Stereo::ConverterDLL::SessionLog::is_enabled() const
{
    return m_is_enabled;
}

void Stereo::ConverterDLL::SessionLog::write(const std::string& in_text) const
{
    if (m_is_enabled)
    {
        write_failure(in_text);
    }
}

void Stereo::ConverterDLL::SessionLog::write_failure(const std::string& in_text) const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_file == INVALID_HANDLE_VALUE)
    {
        m_file = ::CreateFileA(m_log_path.c_str(),
                               FILE_APPEND_DATA,
                               FILE_SHARE_READ | FILE_SHARE_WRITE,
                               nullptr,
                               OPEN_ALWAYS,
                               FILE_ATTRIBUTE_NORMAL,
                               nullptr);
    }

    const HANDLE file = m_file;

    if (file != INVALID_HANDLE_VALUE)
    {
        DWORD             n_written = 0;
        const std::string line      = in_text                                  +
                                      " (process "                             +
                                      std::to_string(::GetCurrentProcessId() ) +
                                      ", "                                     +
                                      m_program_path                           +
                                      ", recipe "                              +
                                      m_recipe_path                            +
                                      ")\r\n";

        ::WriteFile  (file,
                      line.c_str        (),
                      static_cast<DWORD>(line.size() ),
                     &n_written,
                      nullptr);
    }
}
