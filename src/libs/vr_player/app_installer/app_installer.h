/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Puts an application in its directory: downloads its archive, checks it, unpacks it, and moves it into place. It writes only
 * under the directory of the application and next to it (install_directory + ".download", which is removed when it is done), so
 * nothing is left anywhere else. The work is done by a thread of its own; the window asks for the progress.
 */
#ifndef APPINSTALLER_APP_INSTALLER_H
#define APPINSTALLER_APP_INSTALLER_H

#include "utils/files/i_downloader.h"
#include "vr_player/app_installer/types.h"
#include <atomic>
#include <mutex>
#include <string>
#include <thread>

namespace VRPlayer
{
    namespace AppInstaller
    {
        class AppInstaller
        {
        public:
            /* The downloader is not owned, and has to outlive this object. */
            explicit AppInstaller(Utils::Files::IDownloader* in_downloader_ptr);

            /* Stops what is done, and waits for it. */
            ~AppInstaller();

            AppInstaller(const AppInstaller&)            = delete;
            AppInstaller& operator=(const AppInstaller&) = delete;

            /* Asks for what is done to stop; the progress says CANCELLED soon after. */
            void cancel();

            /* A copy of the progress, as of now. */
            InstallProgress get_progress() const;

            /* True from start() until the progress is DONE, FAILED or CANCELLED. */
            bool is_busy() const;

            /* Checks that the application can be put where it goes (the directory is not taken, and the one it is made in can be
             * written to), and starts. Returns false, and says why, if not: that is what the user is told, since nothing is done
             * anywhere else.
             */
            bool start(const InstallSpec& in_spec,
                       std::string*       out_error_ptr);

        private:
            /* What the thread and the window share. The lock is held for a copy of the progress and for nothing else. */
            struct SharedState
            {
                InstallProgress progress;
                std::mutex      progress_mutex;
            };

            void run_install (const InstallSpec&  in_spec);
            void set_progress(const InstallPhase& in_phase,
                              const std::string&  in_message,
                              const uint64_t&     in_n_bytes_done,
                              const uint64_t&     in_n_bytes_total);

            static std::string get_host     (const std::string& in_url);
            static bool        is_sha256_hex(const std::string& in_text);

            Utils::Files::IDownloader*        m_downloader_ptr;
            std::atomic<bool>   m_is_busy;      /* not guarded: set by start() and by the thread, read by the window */
            std::atomic<bool>   m_is_cancelled; /* not guarded: set by the window, read by the thread                */
            mutable SharedState m_shared_state;
            std::thread         m_thread;
        };
    }
}

#endif /* APPINSTALLER_APP_INSTALLER_H */
