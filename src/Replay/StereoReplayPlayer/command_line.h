/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREOREPLAYPLAYER_COMMAND_LINE_H
#define STEREOREPLAYPLAYER_COMMAND_LINE_H

#include "types.h"
#include <string>
#include <vector>

namespace Replay
{
    namespace StereoReplayPlayer
    {
        class CommandLine
        {
        public:
            /* The arguments to give to the 32-bit player, so that it plays what this one was asked to (all but the workload). */
            static std::vector<std::string> get_handover_argument_vec(const Options& in_options);

            static const char* get_usage_ptr();

            /* Returns false, and says why, if the arguments are not right. */
            static bool parse(const int&   in_argc,
                              char*        in_argv[],
                              Options*     out_options_ptr,
                              std::string* out_error_ptr);

        private:
            /* The text is A-B: frames, counted from 0, with B not before A. */
            static bool parse_frame_range(const char* in_text_ptr,
                                          Options*    out_options_ptr);
        };
    }
}

#endif /* STEREOREPLAYPLAYER_COMMAND_LINE_H */
