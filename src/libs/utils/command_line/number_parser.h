/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Parses the numbers of command lines strictly: the whole text must be the number.
 */
#ifndef UTILS_COMMANDLINE_NUMBER_PARSER_H
#define UTILS_COMMANDLINE_NUMBER_PARSER_H

#include <stdint.h>
#include <string>

namespace Utils
{
    namespace CommandLine
    {
        class NumberParser
        {
        public:
            /* Accepts only digits and a decimal point, and only finite results. Returns false and leaves the output unchanged otherwise. */
            static bool parse_double(const std::string& in_text,
                                     double*            out_value_ptr);

            /* Parses the value of an option that takes a number: the argument after the option. Returns false and sets the error ("--limit needs a number",
             * or "... needs a number, not x") if there is no next argument or it is not a valid number up to in_max_value.
             */
            static bool parse_option_unsigned(const std::string& in_option,
                                              const bool&        in_has_next,
                                              const std::string& in_next_argument,
                                              const uint64_t&    in_max_value,
                                              uint64_t*          out_value_ptr,
                                              std::string*       out_error_ptr);

            /* Accepts decimal digits only: no sign, no space, no empty text. Returns false if the value would exceed in_max_value. */
            static bool parse_unsigned(const std::string& in_text,
                                       const uint64_t&    in_max_value,
                                       uint64_t*          out_value_ptr);
        };
    }
}

#endif /* UTILS_COMMANDLINE_NUMBER_PARSER_H */
