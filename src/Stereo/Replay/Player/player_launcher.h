/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_REPLAY_PLAYER_PLAYER_LAUNCHER_H
#define STEREO_REPLAY_PLAYER_PLAYER_LAUNCHER_H

#include "types.h"
#include <string>

namespace Stereo
{
    namespace Replay
    {
        namespace Player
        {
            /* D3D8 is 32-bit only: a workload with D3D8 calls is handed from the 64-bit player to the 32-bit one next to it. */
            class PlayerLauncher
            {
            public:
                /* True if this is the 64-bit player and the workload holds D3D8 calls. False if the file cannot be read: the replay
                 * reports it.
                 */
                static bool is_handover_needed(const char* in_workload_path_ptr);

                /* Runs the 32-bit player with the options and waits. It is killed with this process. False, with the reason, if it
                 * cannot be started.
                 */
                static bool run_x86_player(const Options& in_options,
                                           int*           out_exit_code_ptr,
                                           std::string*   out_error_ptr);

            private:
                /* The text in the ANSI code page, as the command line gives it. */
                static std::wstring get_wide_string(const std::string& in_text);
            };
        }
    }
}

#endif /* STEREO_REPLAY_PLAYER_PLAYER_LAUNCHER_H */
