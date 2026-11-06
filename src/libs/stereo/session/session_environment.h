/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_SESSION_SESSION_ENVIRONMENT_H
#define STEREO_SESSION_SESSION_ENVIRONMENT_H

#include "stereo/session/types.h"
#include <stdint.h>
#include <string>

namespace Stereo
{
    namespace Session
    {
        /* How the launcher hands a playback over to the DLL: through environment variables of its own process, which the application
         * inherits. Each playback has its own, so that several can run, and nothing is left in a directory.
         */
        class SessionEnvironment
        {
        public:
            /* What the application exits with when the converter stops it, since what it is given cannot be converted (see the log). */
            static uint32_t get_converter_failed_exit_code();

            /* Where the converter in the application writes what it has to say. */
            static std::string get_log_path(const std::string& in_session_id);

            /* The DLL's side: false if the process was not started by the launcher. */
            static bool read(SessionInfo* out_info_ptr);

            /* The launcher's side: before the application is started. */
            static bool write(const SessionInfo& in_info);

        private:
            /* False if the variable is not set, or is more than the system allows. */
            static bool read_variable(const char*  in_name_ptr,
                                      std::string* out_value_ptr);

            static const char* const LOG_VARIABLE_NAME;
            static const char* const RECIPE_VARIABLE_NAME;
            static const char* const SESSION_ID_VARIABLE_NAME;
        };
    }
}

#endif /* STEREO_SESSION_SESSION_ENVIRONMENT_H */
