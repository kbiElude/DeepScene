/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * What a program with options of its own says of an argument that the shared part of a command line does not know.
 */
#ifndef UTILS_COMMANDLINE_PROGRAM_OPTION_FUNC_H
#define UTILS_COMMANDLINE_PROGRAM_OPTION_FUNC_H

#include <functional>
#include <string>

namespace Utils
{
    namespace CommandLine
    {
        /* Whether the argument is one of the program's options, whether it takes the argument after it as its value, and, if it is one and what it was given is not
         * right, why (the function returns false then).
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
