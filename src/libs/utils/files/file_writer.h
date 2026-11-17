/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef UTILS_FILES_FILE_WRITER_H
#define UTILS_FILES_FILE_WRITER_H

#include <stddef.h>
#include <string>

namespace Utils
{
    namespace Files
    {
        class FileWriter
        {
        public:
            /* Replaces the file with the bytes. False if it cannot be created, written in full, or closed. */
            static bool write_bytes(const std::string& in_path,
                                    const void*        in_data_ptr,
                                    const size_t&      in_n_bytes);
        };
    }
}

#endif /* UTILS_FILES_FILE_WRITER_H */
