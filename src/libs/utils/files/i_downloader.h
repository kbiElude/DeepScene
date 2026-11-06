/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef UTILS_FILES_I_DOWNLOADER_H
#define UTILS_FILES_I_DOWNLOADER_H

#include "utils/files/progress_callback.h"
#include <atomic>
#include <string>

namespace Utils
{
    namespace Files
    {
        /* Gets one file from an address. It is behind an interface so that what installs does not need a network to be tested. */
        class IDownloader
        {
        public:
            virtual ~IDownloader() = default;

            /* Writes the file at the address to in_file_path. Returns false, and says why, if it could not; a download which
             * in_is_cancelled stops is one of them.
             */
            virtual bool download(const std::string&       in_url,
                                  const std::string&       in_file_path,
                                  const std::atomic<bool>& in_is_cancelled,
                                  const ProgressCallback&  in_progress_func,
                                  std::string*             out_error_ptr) = 0;
        };
    }
}

#endif /* UTILS_FILES_I_DOWNLOADER_H */
