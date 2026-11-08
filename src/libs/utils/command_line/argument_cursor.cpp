/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "utils/command_line/argument_cursor.h"

Utils::CommandLine::ArgumentCursor::ArgumentCursor(const int& in_argc,
                                                   char*      in_argv[],
                                                   const int& in_n_argument)
    :m_argument     (in_argv[in_n_argument]),
     m_has_next     (in_n_argument + 1 < in_argc),
     m_next_argument( (in_n_argument + 1 < in_argc) ? in_argv[in_n_argument + 1]
                                                    : "")
{
    /* Stub */
}

bool Utils::CommandLine::ArgumentCursor::ask(const ProgramOptionFunc& in_program_option_func,
                                             bool*                    out_is_known_ptr,
                                             bool*                    out_uses_next_ptr,
                                             std::string*             out_error_ptr) const
{
    bool result = true;

    *out_is_known_ptr  = false;
    *out_uses_next_ptr = false;

    if (in_program_option_func)
    {
        result = in_program_option_func(m_argument,
                                        m_has_next,
                                        m_next_argument,
                                        out_is_known_ptr,
                                        out_uses_next_ptr,
                                        out_error_ptr);
    }

    return result;
}

const std::string& Utils::CommandLine::ArgumentCursor::get_argument() const
{
    return m_argument;
}

const std::string& Utils::CommandLine::ArgumentCursor::get_next_argument() const
{
    return m_next_argument;
}

bool Utils::CommandLine::ArgumentCursor::has_next() const
{
    return m_has_next;
}
