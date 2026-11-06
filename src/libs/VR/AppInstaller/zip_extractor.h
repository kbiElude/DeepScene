/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef APPINSTALLER_ZIP_EXTRACTOR_H
#define APPINSTALLER_ZIP_EXTRACTOR_H

#include "VR/AppInstaller/progress_callback.h"
#include <atomic>
#include <stdint.h>
#include <string>

namespace VR
{
    namespace AppInstaller
    {
        /* Unpacks a zip file (with miniz) into a directory, and does not trust it: a name which would put a file outside the directory
         * (a path of a drive, "..", a name of a device), an entry which is encrypted, files which together are more than the limit, and
         * names which are the same in the eyes of the system all fail the unpacking, and a file which is longer than its entry says
         * is not written beyond it.
         */
        class ZipExtractor
        {
        public:
            /* The entries go under in_directory, which is made. Returns false, and says why, if they cannot; what was written is left. */
            static bool extract(const std::string&       in_archive_path,
                                const std::string&       in_directory,
                                const uint64_t&          in_n_unpacked_bytes_max,
                                const std::atomic<bool>& in_is_cancelled,
                                const ProgressCallback&  in_progress_func,
                                std::string*             out_error_ptr);

            /* True if the name of an entry stays inside the directory it is unpacked into. */
            static bool is_name_safe(const std::string& in_name);

        private:
            /* What the writing of an entry needs; it is the opaque pointer of the write callback of miniz. */
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

            static std::string to_lower(const std::string& in_text);

            static size_t write(void*       in_context_ptr,
                                uint64_t    in_offset,
                                const void* in_bytes_ptr,
                                size_t      in_n_bytes);
        };
    }
}

#endif /* APPINSTALLER_ZIP_EXTRACTOR_H */
