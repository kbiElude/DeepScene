/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "Stereo/Session/session_environment.h"
#include <Windows.h>

const char* const Stereo::Session::SessionEnvironment::LOG_VARIABLE_NAME        = "APISTEREO_LOG";
const char* const Stereo::Session::SessionEnvironment::RECIPE_VARIABLE_NAME     = "APISTEREO_RECIPE";
const char* const Stereo::Session::SessionEnvironment::SESSION_ID_VARIABLE_NAME = "APISTEREO_SESSION";


uint32_t Stereo::Session::SessionEnvironment::get_converter_failed_exit_code()
{
    constexpr uint32_t EXIT_CODE = 3;

    return EXIT_CODE;
}

std::string Stereo::Session::SessionEnvironment::get_log_path(const std::string& in_session_id)
{
    std::string result;
    char        temp_i8vec260[MAX_PATH] = {};

    if (::GetTempPathA(MAX_PATH,
                       temp_i8vec260) != 0)
    {
        result = std::string(temp_i8vec260) + "apistereo_" + in_session_id + ".log";
    }

    return result;
}

bool Stereo::Session::SessionEnvironment::read(SessionInfo* out_info_ptr)
{
    std::string log_value;

    /* Whether to log is not needed: a process that is not told to does not. */
    out_info_ptr->is_logging = (read_variable(LOG_VARIABLE_NAME,
                                              &log_value) &&
                                log_value == "1");

    return (read_variable(RECIPE_VARIABLE_NAME,
                          &out_info_ptr->recipe_path) &&
            read_variable(SESSION_ID_VARIABLE_NAME,
                          &out_info_ptr->session_id) );
}

bool Stereo::Session::SessionEnvironment::read_variable(const char*  in_name_ptr,
                                                        std::string* out_value_ptr)
{
    constexpr DWORD N_MAX_CHARACTERS = 32768; /* what the system allows in an environment variable */

    std::string buffer(N_MAX_CHARACTERS,
                       '\0');
    const DWORD n_characters = ::GetEnvironmentVariableA(in_name_ptr,
                                                         &buffer[0],
                                                         N_MAX_CHARACTERS);
    const bool  result       = (n_characters > 0 &&
                                n_characters < N_MAX_CHARACTERS);

    if (result)
    {
        *out_value_ptr = buffer.substr(0,
                                       n_characters);
    }

    return result;
}

bool Stereo::Session::SessionEnvironment::write(const SessionInfo& in_info)
{
    /* No variable is a process that does not log, so a value from an earlier session of this process is taken away. */
    ::SetEnvironmentVariableA(LOG_VARIABLE_NAME,
                              (in_info.is_logging) ? "1"
                                                   : nullptr);

    return (::SetEnvironmentVariableA(RECIPE_VARIABLE_NAME,
                                      in_info.recipe_path.c_str() ) != FALSE &&
            ::SetEnvironmentVariableA(SESSION_ID_VARIABLE_NAME,
                                      in_info.session_id.c_str() ) != FALSE);
}
