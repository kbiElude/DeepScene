/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * The loop over the arguments of a command line: each is given to a function, which says whether it took the argument after it as its value (that one is then not
 * given on its own).
 */
#ifndef UTILS_COMMANDLINE_ARGUMENT_WALKER_H
#define UTILS_COMMANDLINE_ARGUMENT_WALKER_H

#include "utils/command_line/argument_cursor.h"
#include <functional>
#include <string>

namespace Utils
{
    namespace CommandLine
    {
        class ArgumentWalker
        {
        public:
            /* What a program does with an argument: false, and the reason, if the argument is not right. Whether it took the argument after it is set. */
            typedef std::function<bool(const ArgumentCursor& in_cursor,
                                       bool*                 out_uses_next_ptr,
                                       std::string*          out_error_ptr)> HandlerFunc;

            /* Gives the arguments (not the name of the program) to the function in order. False, and the reason, if the function says an argument is not right. */
            static bool walk(const int&         in_argc,
                             char*              in_argv[],
                             const HandlerFunc& in_handler_func,
                             std::string*       out_error_ptr);
        };
    }
}

#endif /* UTILS_COMMANDLINE_ARGUMENT_WALKER_H */
