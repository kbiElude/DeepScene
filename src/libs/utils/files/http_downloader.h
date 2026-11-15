/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef UTILS_FILES_HTTP_DOWNLOADER_H
#define UTILS_FILES_HTTP_DOWNLOADER_H

#include "utils/files/i_downloader.h"

namespace Utils
{
    namespace Files
    {
        /* Downloads with WinHTTP, which is part of Windows and brings its proxy settings, certificates and TLS. Addresses are https. A redirect to
         * an http address is not followed (this is the WinHTTP default).
         */
        class HttpDownloader : public IDownloader
        {
        public:
            /* An http address is refused unless in_is_http_allowed is set (the tests set it because they talk to their own server). A read that waits
             * longer than in_receive_timeout_ms for the server ends the download. This is also the longest a cancel can take.
             */
            HttpDownloader(const bool& in_is_http_allowed,
                           const int&  in_receive_timeout_ms = 5000);

            bool download(const std::string&       in_url,
                          const std::string&       in_file_path,
                          const std::atomic<bool>& in_is_cancelled,
                          const ProgressCallback&  in_progress_func,
                          std::string*             out_error_ptr) override;

        private:
            bool m_is_http_allowed;
            int  m_receive_timeout_ms;
        };
    }
}

#endif /* UTILS_FILES_HTTP_DOWNLOADER_H */
