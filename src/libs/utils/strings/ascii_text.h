/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Helpers for ASCII text, shared by the programs of this project.
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
            /* Returns the text with its ASCII letters converted to lower case. All other bytes are kept. */
            static std::string to_lower(const std::string& in_text);
        };
    }
}

#endif /* UTILS_STRINGS_ASCII_TEXT_H */
