/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef APPINSTALLER_HTTP_DOWNLOADER_H
#define APPINSTALLER_HTTP_DOWNLOADER_H

#include "VR/AppInstaller/i_downloader.h"

namespace VR
{
    namespace AppInstaller
    {
        /* Downloads with WinHTTP, which is part of the system: its proxy settings, its certificates, its TLS. An address is https; a
         * redirect to an address which is http is not followed (the default of WinHTTP, which is kept).
         */
        class HttpDownloader : public IDownloader
        {
        public:
            /* An address which is http is refused, unless in_is_http_allowed (for the tests, which talk to a server of their own). A read
             * which waits longer than in_receive_timeout_ms for the server ends the download, and so is how long a cancel can take.
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

#endif /* APPINSTALLER_HTTP_DOWNLOADER_H */
