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
            CANCELLED,
            DONE,
            DOWNLOADING,
            EXTRACTING,
            FAILED,
            IDLE,
            VERIFYING
        };

        /* Where an application is downloaded from and what its archive must look like. */
        struct InstallSpec
        {
            std::string              install_directory;        /* absolute: where the application files go; the installer creates it, so it must not exist beforehand */
            uint64_t                 n_archive_bytes      = 0; /* size of the download; 0 if unknown                                                */
            uint64_t                 n_unpacked_bytes_max = 0; /* the most that the unpacked files may take together                                */
            std::string              page_url;                 /* where the archive can be downloaded by hand; shown if the download fails          */
            std::string              sha256_hex;               /* SHA-256 of the archive as lower-case hex; an archive with another hash is not unpacked */
            std::vector<std::string> url_vec;                  /* addresses of the archive, tried in order until one works                          */
        };

        struct InstallProgress
        {
            std::string  message;           /* what is being done, or why it failed */
            uint64_t     n_bytes_done  = 0; /* within the current phase                   */
            uint64_t     n_bytes_total = 0; /* of the current phase; 0 if unknown         */
            InstallPhase phase         = InstallPhase::IDLE;
        };
    }
}

#endif /* APPINSTALLER_TYPES_H */
