/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_SESSION_TYPES_H
#define STEREO_SESSION_TYPES_H

#include <string>

namespace Stereo
{
    namespace Session
    {
        /* What the launcher tells the DLL about the playback it is injected for (see SessionEnvironment). */
        struct SessionInfo
        {
            bool        is_logging = false; /* the converter writes what it does to the log of the session; otherwise only why it stops        */
            std::string recipe_path;
            std::string session_id;        /* names what the DLL and the program which shows its images share         */
        };
    }
}

#endif /* STEREO_SESSION_TYPES_H */
