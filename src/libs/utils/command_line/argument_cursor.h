/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * One argument of a command line, with the one after it (which an option may take as its value).
 */
#ifndef UTILS_COMMANDLINE_ARGUMENT_CURSOR_H
#define UTILS_COMMANDLINE_ARGUMENT_CURSOR_H

#include "utils/command_line/program_option_func.h"
#include <string>

namespace Utils
{
    namespace CommandLine
    {
        class ArgumentCursor
        {
        public:
            /* The argument at the index (the first is 1: 0 is the program). */
            ArgumentCursor(const int& in_argc,
                           char*      in_argv[],
                           const int& in_n_argument);

            /* Asks the function of the program (if it has one) about the argument. False, and the reason, if the program says that what it was given is not right.
             * Whether the argument is one of the program's, and whether it takes the next one, are set (false, false if there is no function).
             */
            bool ask(const ProgramOptionFunc& in_program_option_func,
                     bool*                    out_is_known_ptr,
                     bool*                    out_uses_next_ptr,
                     std::string*             out_error_ptr) const;

            const std::string& get_argument     () const;
            const std::string& get_next_argument() const;
            bool               has_next         () const;

        private:
            std::string m_argument;
            bool        m_has_next;
            std::string m_next_argument;
        };
    }
}

#endif /* UTILS_COMMANDLINE_ARGUMENT_CURSOR_H */
