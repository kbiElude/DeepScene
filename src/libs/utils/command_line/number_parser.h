/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Reads the numbers of command lines strictly: the whole text must be the number.
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
            /* Returns false, and leaves the output alone, for text that is not a number which is not above the maximum. */
            static bool parse_double(const std::string& in_text,
                                     double*            out_value_ptr);

            /* Decimal digits and nothing else: no sign, no space, no empty text. */
            static bool parse_unsigned(const std::string& in_text,
                                       const uint64_t&    in_max_value,
                                       uint64_t*          out_value_ptr);
        };
    }
}

#endif /* UTILS_COMMANDLINE_NUMBER_PARSER_H */
