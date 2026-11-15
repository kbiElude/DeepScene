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
            bool        is_logging = false; /* the converter logs everything it does; otherwise it logs only why it stops */
            std::string recipe_path;
            std::string session_id;        /* names the resources that the DLL and the viewer share                      */
        };
    }
}

#endif /* STEREO_SESSION_TYPES_H */
