/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef UTILS_FILES_WINDOWS_PATH_ENCODING_H
#define UTILS_FILES_WINDOWS_PATH_ENCODING_H

#include <string>

namespace Utils
{
    namespace Files
    {
        /* Converts between the UTF-8 that the programs of this project use for names and the UTF-16 that Windows takes. */
        class WindowsPathEncoding
        {
        public:
            static std::string to_utf8(const std::wstring& in_text);

            /* Converts a path to the form Windows expects: backslash separators, and for a long path the prefix that lifts the length limit. */
            static std::wstring to_wide(const std::string& in_text);

            /* Converts text to UTF-16 without changing it. For text that is not a path, such as an address or a name. */
            static std::wstring to_wide_text(const std::string& in_text);
        };
    }
}

#endif /* UTILS_FILES_WINDOWS_PATH_ENCODING_H */
