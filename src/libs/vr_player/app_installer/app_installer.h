/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Installs an application into its directory: downloads its archive, verifies it, unpacks it, and moves it into place. It writes only to the
 * application's directory and to a staging directory next to it (install_directory + ".download", removed when the installation ends), so
 * nothing is left anywhere else. The work runs on a thread of its own, and the window asks for the progress.
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
            /* The downloader is not owned and must outlive this object. */
            explicit AppInstaller(Utils::Files::IDownloader* in_downloader_ptr);

            /* Stops a running installation, if any, and waits for it. */
            ~AppInstaller();

            AppInstaller(const AppInstaller&)            = delete;
            AppInstaller& operator=(const AppInstaller&) = delete;

            /* Asks the installation to stop. The progress changes to CANCELLED shortly afterwards. */
            void cancel();

            /* Returns a copy of the current progress. */
            InstallProgress get_progress() const;

            /* True from start() until the progress is DONE, FAILED or CANCELLED. */
            bool is_busy() const;

            /* Checks that the application can be installed (the install directory does not exist yet and its parent directory can be written to),
             * then starts the installation. Returns false and sets the error otherwise. The error is what the user is told, because nothing is
             * written anywhere else.
             */
            bool start(const InstallSpec& in_spec,
                       std::string*       out_error_ptr);

        private:
            /* State shared by the installer thread and the window. The lock is held only while the progress is copied. */
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

            Utils::Files::IDownloader* m_downloader_ptr;
            std::atomic<bool>          m_is_busy;      /* not guarded: set by start() and by the thread, read by the window */
            std::atomic<bool>          m_is_cancelled; /* not guarded: set by the window, read by the thread                */
            mutable SharedState        m_shared_state;
            std::thread                m_thread;
        };
    }
}

#endif /* APPINSTALLER_APP_INSTALLER_H */
