/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * How a program handles an argument that the shared command-line code does not know. Used by programs that have options of their own.
 */
#ifndef UTILS_COMMANDLINE_PROGRAM_OPTION_FUNC_H
#define UTILS_COMMANDLINE_PROGRAM_OPTION_FUNC_H

#include <functional>
#include <string>

namespace Utils
{
    namespace CommandLine
    {
        /* Reports whether the argument is one of the program's options (out_is_known_ptr) and whether it takes the following argument as its value
         * (out_uses_next_ptr). If it is a known option but its value is invalid, the function returns false and sets the reason in out_error_ptr.
         */
        typedef std::function<bool(const std::string& in_argument,
                                   const bool&        in_has_next,
                                   const std::string& in_next_argument,
                                   bool*              out_is_known_ptr,
                                   bool*              out_uses_next_ptr,
                                   std::string*       out_error_ptr)> ProgramOptionFunc;
    }
}

#endif /* UTILS_COMMANDLINE_PROGRAM_OPTION_FUNC_H */
