/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef VRPLAYER_COMMAND_LINE_H
#define VRPLAYER_COMMAND_LINE_H

#include "types.h"
#include <string>

namespace VRPlayer
{
    class CommandLine
    {
    public:
        /* Returns false and sets the error if the arguments are invalid. The arguments do not include the program name. */
        static bool parse(const std::vector<std::string>& in_argument_vec,
                          Options*                        out_options_ptr,
                          std::string*                    out_error_ptr);

        static const char* get_usage_ptr();
    };
}

#endif /* VRPLAYER_COMMAND_LINE_H */
