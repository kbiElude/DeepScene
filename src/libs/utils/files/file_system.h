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
        /* The files, as the programs of this project ask for them: names are UTF-8, and paths may be longer than the old limit. */
        class FileSystem
        {
        public:
            /* Makes the directory, and the ones above it which are not there. */
            static bool create_directories(const std::string& in_path,
                                           std::string*       out_error_ptr);

            static bool is_directory(const std::string& in_path);

            /* True if the directory has nothing in it. */
            static bool is_directory_empty(const std::string& in_path);

            static bool is_file(const std::string& in_path);

            /* Removes the directory, with everything in it. Returns true also if it is not there. */
            static bool remove_tree(const std::string& in_path);

            /* False, and the reason, if a file cannot be made in the directory: what the user is told when the program is where it
             * cannot write.
             */
            static bool test_write(const std::string& in_directory,
                                   std::string*       out_error_ptr);

            static std::string to_utf8(const std::wstring& in_text);

            /* The path as the system wants it: the separators are backslashes, and a long one has the prefix which lifts the limit. */
            static std::wstring to_wide(const std::string& in_text);

            /* The text as it is, in UTF-16: an address, a name. */
            static std::wstring to_wide_text(const std::string& in_text);
        };
    }
}

#endif /* UTILS_FILES_FILE_SYSTEM_H */
