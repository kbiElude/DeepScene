/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_REPLAY_PLAYER_COMMAND_LINE_H
#define STEREO_REPLAY_PLAYER_COMMAND_LINE_H

#include "types.h"
#include <string>
#include <vector>

namespace Stereo
{
    namespace Replay
    {
        namespace Player
        {
            class CommandLine
            {
            public:
                /* Returns the arguments to pass to the 32-bit player so that it plays what this player was asked to play. The
                 * workload is not included.
                 */
                static std::vector<std::string> get_handover_argument_vec(const Options& in_options);

                static const char* get_usage_ptr();

                /* Returns false, and sets the reason in out_error_ptr, if the arguments are invalid. */
                static bool parse(const int&   in_argc,
                                  char*        in_argv[],
                                  Options*     out_options_ptr,
                                  std::string* out_error_ptr);

            private:
                /* Parses a frame range written as A-B (frames counted from 0, B not before A). Returns false if the text is not a
                 * valid range.
                 */
                static bool parse_frame_range(const char* in_text_ptr,
                                              Options*    out_options_ptr);
            };
        }
    }
}

#endif /* STEREO_REPLAY_PLAYER_COMMAND_LINE_H */
