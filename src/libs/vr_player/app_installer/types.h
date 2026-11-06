/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef APPINSTALLER_TYPES_H
#define APPINSTALLER_TYPES_H

#include <stdint.h>
#include <string>
#include <vector>

namespace VRPlayer
{
    namespace AppInstaller
    {
        enum class InstallPhase
        {
            IDLE,
            DOWNLOADING,
            VERIFYING,
            EXTRACTING,
            DONE,
            FAILED,
            CANCELLED
        };

        /* Where an application comes from, and what the archive it comes in has to be. */
        struct InstallSpec
        {
            std::string              install_directory;        /* absolute: where the files of the application go; it is made, and is not there yet */
            uint64_t                 n_archive_bytes      = 0; /* what the download is; 0 if it is not known                                        */
            uint64_t                 n_unpacked_bytes_max = 0; /* what the files may take together                                                  */
            std::string              page_url;                 /* where the archive can be got by hand; shown if the download does not work         */
            std::string              sha256_hex;               /* of the archive, in lower case: what is not that is not unpacked                   */
            std::vector<std::string> url_vec;                  /* of the archive, one after the other until one works                               */
        };

        struct InstallProgress
        {
            std::string  message;           /* what is being done, or why it did not work */
            uint64_t     n_bytes_done  = 0; /* of the phase                               */
            uint64_t     n_bytes_total = 0; /* of the phase; 0 if it is not known         */
            InstallPhase phase         = InstallPhase::IDLE;
        };
    }
}

#endif /* APPINSTALLER_TYPES_H */
