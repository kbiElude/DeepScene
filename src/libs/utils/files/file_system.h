/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef UTILS_FILES_FILE_SYSTEM_H
#define UTILS_FILES_FILE_SYSTEM_H

#include <string>

namespace Utils
{
    namespace Files
    {
        /* File operations as the programs of this project use them: names are UTF-8, and paths may be longer than MAX_PATH. */
        class FileSystem
        {
        public:
            /* Creates the directory and any missing parent directories. */
            static bool create_directories(const std::string& in_path,
                                           std::string*       out_error_ptr);

            static bool is_directory(const std::string& in_path);

            /* True if the directory has nothing in it. */
            static bool is_directory_empty(const std::string& in_path);

            static bool is_file(const std::string& in_path);

            /* Removes the directory with everything in it. Also returns true if the directory does not exist. */
            static bool remove_tree(const std::string& in_path);

            /* Returns false and sets the error if a file cannot be created in the directory. The error is what the program shows the user when it cannot
             * write there.
             */
            static bool test_write(const std::string& in_directory,
                                   std::string*       out_error_ptr);

            static std::string to_utf8(const std::wstring& in_text);

            /* Converts a path to the form Windows expects: backslash separators, and for a long path the prefix that lifts the length limit. */
            static std::wstring to_wide(const std::string& in_text);

            /* Converts text to UTF-16 without changing it. For text that is not a path, such as an address or a name. */
            static std::wstring to_wide_text(const std::string& in_text);
        };
    }
}

#endif /* UTILS_FILES_FILE_SYSTEM_H */
