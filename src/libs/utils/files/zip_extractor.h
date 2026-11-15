/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef UTILS_FILES_ZIP_EXTRACTOR_H
#define UTILS_FILES_ZIP_EXTRACTOR_H

#include "utils/files/progress_callback.h"
#include <atomic>
#include <stdint.h>
#include <string>

namespace Utils
{
    namespace Files
    {
        /* Unpacks a zip file into a directory with miniz and does not trust the archive. These cases make the extraction fail: a name that would
         * place a file outside the directory (a drive path, "..", a device name), an encrypted entry, files whose total size exceeds the limit,
         * and names that Windows treats as the same. A file longer than its entry declares is not written beyond the declared size.
         */
        class ZipExtractor
        {
        public:
            /* Writes the entries under in_directory, which is created. Returns false and sets the error on failure. Files already written are left in place. */
            static bool extract(const std::string&       in_archive_path,
                                const std::string&       in_directory,
                                const uint64_t&          in_n_unpacked_bytes_max,
                                const std::atomic<bool>& in_is_cancelled,
                                const ProgressCallback&  in_progress_func,
                                std::string*             out_error_ptr);

            /* True if an entry name stays inside the directory it is unpacked into. */
            static bool is_name_safe(const std::string& in_name);

        private:
            /* The state needed to write one entry. It is passed as the opaque context pointer of the miniz write callback. */
            struct WriteContext
            {
                void*                    file_handle_ptr; /* HANDLE */
                const std::atomic<bool>* is_cancelled_ptr;
                uint64_t                 n_bytes_declared;
                uint64_t                 n_bytes_total;
                uint64_t                 n_bytes_total_done;
                uint64_t                 n_bytes_written;
                const ProgressCallback*  progress_func_ptr;
            };

            static size_t write(void*       in_context_ptr,
                                uint64_t    in_offset,
                                const void* in_bytes_ptr,
                                size_t      in_n_bytes);
        };
    }
}

#endif /* UTILS_FILES_ZIP_EXTRACTOR_H */
