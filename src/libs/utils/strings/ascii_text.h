/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * What is done to text whose letters are ASCII (file names, keys, hash digits), the same for everything that needs it.
 */
#ifndef UTILS_STRINGS_ASCII_TEXT_H
#define UTILS_STRINGS_ASCII_TEXT_H

#include <string>

namespace Utils
{
    namespace Strings
    {
        class AsciiText
        {
        public:
            /* The text with its letters in lower case (the letters of the ASCII range; any other byte is left as it is). */
            static std::string to_lower(const std::string& in_text);
        };
    }
}

#endif /* UTILS_STRINGS_ASCII_TEXT_H */
