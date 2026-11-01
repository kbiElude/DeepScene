/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREOCONVERTERDLL_SESSION_LOG_H
#define STEREOCONVERTERDLL_SESSION_LOG_H

#include "Stereo/StereoSession/types.h"
#include <mutex>
#include <string>
#include <Windows.h>

namespace Stereo
{
    namespace StereoConverterDLL
    {
        /* The log of the session, which the programs around the application read (see SessionEnvironment::get_log_path()): a line says
         * what the converter did, with the process and the recipe it is of. Any thread. Nothing is written unless the session asks for a log
         * (see SessionInfo::is_logging), except what has to be said whatever is asked: why the converter stops the application, and what
         * keeps it from showing the application. The file is kept open: opening and closing it for
         * each line, which a program that watches files then looks at, took the application as long as a frame.
         */
        class SessionLog
        {
        public:
            explicit SessionLog(const Stereo::StereoSession::SessionInfo& in_session_info);

            ~SessionLog();

            SessionLog(const SessionLog&)            = delete;
            SessionLog& operator=(const SessionLog&) = delete;

            /* Whether what the converter does is written (see write()). */
            bool is_enabled() const;

            /* A line, if the session logs. */
            void write(const std::string& in_text) const;

            /* A line that is written whether the session logs or not: what has gone wrong. */
            void write_failure(const std::string& in_text) const;

        private:
            mutable HANDLE     m_file;
            bool               m_is_enabled;
            std::string        m_log_path;
            mutable std::mutex m_mutex; /* guards the file */
            std::string        m_program_path;
            std::string        m_recipe_path;
        };
    }
}

#endif /* STEREOCONVERTERDLL_SESSION_LOG_H */
