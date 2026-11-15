/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * One argument of a command line, together with the argument after it (an option may take that one as its value).
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
            /* Points at the argument with index in_n_argument. Counting starts at 1 because index 0 is the program name. */
            ArgumentCursor(const int& in_argc,
                           char*      in_argv[],
                           const int& in_n_argument);

            /* Asks the program's option function (if it has one) about the argument. Returns false and sets the error if the function rejects the value
             * it was given. Otherwise it sets out_is_known_ptr (the argument is one of the program's options) and out_uses_next_ptr (the option takes
             * the next argument as its value). Both are false if there is no function.
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
