/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Loops over the arguments of a command line and gives each one to a handler function. The handler reports whether it used the following argument
 * as its value. That argument is then not passed to the handler on its own.
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
            /* Handles one argument. Returns false and sets the error if the argument is invalid. Sets out_uses_next_ptr if it used the following argument. */
            typedef std::function<bool(const ArgumentCursor& in_cursor,
                                       bool*                 out_uses_next_ptr,
                                       std::string*          out_error_ptr)> HandlerFunc;

            /* Calls the handler for each argument in order (the program name is skipped). Returns false and sets the error if the handler rejects an argument. */
            static bool walk(const int&         in_argc,
                             char*              in_argv[],
                             const HandlerFunc& in_handler_func,
                             std::string*       out_error_ptr);
        };
    }
}

#endif /* UTILS_COMMANDLINE_ARGUMENT_WALKER_H */
