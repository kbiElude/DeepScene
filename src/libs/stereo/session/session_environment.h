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
        /* Hands a playback from the launcher to the DLL through environment variables of the launcher's own process. The application
         * inherits them. Each playback has its own session, so several playbacks can run at once, and nothing is left in a directory.
         */
        class SessionEnvironment
        {
        public:
            /* The exit code of the application when the converter stops it because it cannot convert what the application does (the log says
             * why).
             */
            static uint32_t get_converter_failed_exit_code();

            /* The file in which the converter inside the application writes its messages. */
            static std::string get_log_path(const std::string& in_session_id);

            /* The DLL's side. Reads the session info. Returns false if the process was not started by the launcher. */
            static bool read(SessionInfo* out_info_ptr);

            /* The launcher's side. Call it before the application is started. Returns false if a variable cannot be set. */
            static bool write(const SessionInfo& in_info);

        private:
            /* Returns false if the variable is not set or its value is longer than the system allows. */
            static bool read_variable(const char*  in_name_ptr,
                                      std::string* out_value_ptr);

            static const char* const LOG_VARIABLE_NAME;
            static const char* const RECIPE_VARIABLE_NAME;
            static const char* const SESSION_ID_VARIABLE_NAME;
        };
    }
}

#endif /* STEREO_SESSION_SESSION_ENVIRONMENT_H */
