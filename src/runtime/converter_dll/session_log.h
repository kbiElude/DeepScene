/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_CONVERTERDLL_SESSION_LOG_H
#define STEREO_CONVERTERDLL_SESSION_LOG_H

#include "stereo/session/types.h"
#include <mutex>
#include <string>
#include <Windows.h>

namespace Stereo
{
    namespace ConverterDLL
    {
        /* The log of the session. The programs around the application read it (see SessionEnvironment::get_log_path()). Each line says what the
         * converter did and names the process and the recipe. Any thread can write to it. Nothing is written unless the session asks for a log
         * (see SessionInfo::is_logging), except messages that must always appear: why the converter stops the application, and what prevents it
         * from showing the application. The file stays open, because opening and closing it for each line would cost the application as much as a
         * frame (a program that watches files reacts to every close).
         */
        class SessionLog
        {
        public:
            explicit SessionLog(const Stereo::Session::SessionInfo& in_session_info);

            ~SessionLog();

            SessionLog(const SessionLog&)            = delete;
            SessionLog& operator=(const SessionLog&) = delete;

            /* Returns true if the session asks for a log,, so write() writes. */
            bool is_enabled() const;

            /* Writes one line if the session logs. */
            void write(const std::string& in_text) const;

            /* Writes one line whether the session logs or not. It is for failures. */
            void write_failure(const std::string& in_text) const;

        private:
            mutable HANDLE     m_file;
            bool               m_is_enabled;
            std::string        m_log_path;
            mutable std::mutex m_mutex; /* guards the file
 */
            std::string        m_program_path;
            std::string        m_recipe_path;
        };
    }
}

#endif /* STEREO_CONVERTERDLL_SESSION_LOG_H */
