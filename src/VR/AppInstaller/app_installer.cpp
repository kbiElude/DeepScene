/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "VR/AppInstaller/app_installer.h"
#include "VR/AppInstaller/file_system.h"
#include "VR/AppInstaller/sha256.h"
#include "VR/AppInstaller/zip_extractor.h"
#include <Windows.h>
#include <algorithm>

VR::AppInstaller::AppInstaller::AppInstaller(IDownloader* in_downloader_ptr)
    :m_downloader_ptr(in_downloader_ptr),
     m_is_busy       (false),
     m_is_cancelled  (false)
{
    /* Nothing to do. */
}

VR::AppInstaller::AppInstaller::~AppInstaller()
{
    cancel();

    if (m_thread.joinable() )
    {
        m_thread.join();
    }
}

void VR::AppInstaller::AppInstaller::cancel()
{
    m_is_cancelled = true;
}

VR::AppInstaller::InstallProgress VR::AppInstaller::AppInstaller::get_progress() const
{
    InstallProgress result;

    {
        std::lock_guard<std::mutex> lock(m_state.mutex);

        result = m_state.progress;
    }

    return result;
}

bool VR::AppInstaller::AppInstaller::is_busy() const
{
    return m_is_busy.load();
}

void VR::AppInstaller::AppInstaller::run_install(const InstallSpec& in_spec)
{
    const std::string                 archive_path     = in_spec.install_directory + ".download\\archive.zip";
    std::string                       error;
    std::string                       error_list;
    DWORD                             lock_error       = 0;
    std::string                       hex;
    HANDLE                            lock_handle      = nullptr;
    std::string                       lock_name;
    std::string                       staging_directory = in_spec.install_directory + ".download";
    std::string                       unpacked_directory = staging_directory + "\\unpacked";
    bool                              is_downloaded    = false;
    bool                              is_ok            = false;
    ProgressCallback                  download_func;
    ProgressCallback                  extract_func;
    uint64_t                          hash             = 14695981039346656037ull;

    /* One installation of a directory at a time, whichever Demo VR Player it is started by. */
    {
        std::string lower_directory = in_spec.install_directory;

        std::transform(lower_directory.begin(),
                       lower_directory.end  (),
                       lower_directory.begin(),
                       ::tolower);

        for (size_t n_character = 0;
                    n_character < lower_directory.size();
                  ++n_character)
        {
            hash = (hash ^ static_cast<unsigned char>(lower_directory.at(n_character) ) ) * 1099511628211ull;
        }

        lock_name = "Local\\DemoVRPlayer.Install." + std::to_string(hash);
    }

    lock_handle = ::CreateMutexW(nullptr,
                                 FALSE,
                                 FileSystem::to_wide(lock_name).c_str() );

    lock_error = ::GetLastError();

    if (lock_handle == nullptr                ||
        lock_error  == ERROR_ALREADY_EXISTS)
    {
        error = "Another Demo VR Player is installing this application.";

        goto end;
    }

    FileSystem::remove_tree(staging_directory);

    if (FileSystem::create_directories(staging_directory,
                                       &error) == false)
    {
        goto end;
    }

    download_func = [this, &in_spec](const uint64_t& in_n_bytes_done,
                                     const uint64_t& in_n_bytes_total)
    {
        set_progress(InstallPhase::DOWNLOADING,
                     "Downloading",
                     in_n_bytes_done,
                     (in_n_bytes_total != 0) ? in_n_bytes_total
                                             : in_spec.n_archive_bytes);
    };

    for (size_t n_url = 0;
                n_url < in_spec.url_vec.size();
              ++n_url)
    {
        std::string url_error;

        set_progress(InstallPhase::DOWNLOADING,
                     "Downloading from " + get_host(in_spec.url_vec.at(n_url) ),
                     0,
                     in_spec.n_archive_bytes);

        if (m_downloader_ptr->download(in_spec.url_vec.at(n_url),
                                       archive_path,
                                       m_is_cancelled,
                                       download_func,
                                      &url_error) )
        {
            is_downloaded = true;

            break;
        }

        if (m_is_cancelled.load() )
        {
            goto end;
        }

        error_list += "\n" + get_host(in_spec.url_vec.at(n_url) ) + ": " + url_error;
    }

    if (is_downloaded == false)
    {
        error = "The application could not be downloaded." + error_list;

        if (in_spec.page_url.empty() == false)
        {
            error += "\n\nIt can be downloaded by hand from " + in_spec.page_url;
        }

        goto end;
    }

    set_progress(InstallPhase::VERIFYING,
                 "Checking the download",
                 0,
                 0);

    if (Sha256::hash_file(archive_path,
                          m_is_cancelled,
                         &hex,
                         &error) == false)
    {
        goto end;
    }

    if (hex != in_spec.sha256_hex)
    {
        error = "The download is not the file which was expected, so it was not used.";

        goto end;
    }

    extract_func = [this](const uint64_t& in_n_bytes_done,
                          const uint64_t& in_n_bytes_total)
    {
        set_progress(InstallPhase::EXTRACTING,
                     "Unpacking",
                     in_n_bytes_done,
                     in_n_bytes_total);
    };

    set_progress(InstallPhase::EXTRACTING,
                 "Unpacking",
                 0,
                 0);

    if (ZipExtractor::extract(archive_path,
                              unpacked_directory,
                              in_spec.n_unpacked_bytes_max,
                              m_is_cancelled,
                              extract_func,
                             &error) == false)
    {
        goto end;
    }

    /* The directory is made only now, whole: an installation which did not end leaves nothing which looks like one. */
    if (FileSystem::is_directory(in_spec.install_directory) )
    {
        ::RemoveDirectoryW(FileSystem::to_wide(in_spec.install_directory).c_str() );
    }

    if (::MoveFileExW(FileSystem::to_wide(unpacked_directory).c_str(),
                      FileSystem::to_wide(in_spec.install_directory).c_str(),
                      0) == FALSE)
    {
        error = "Cannot put the application in " + in_spec.install_directory + " (error " + std::to_string(::GetLastError() ) + ")";

        goto end;
    }

    is_ok = true;

end:
    FileSystem::remove_tree(staging_directory);

    if (lock_handle != nullptr)
    {
        ::CloseHandle(lock_handle);
    }

    if (is_ok)
    {
        set_progress(InstallPhase::DONE,
                     "Installed",
                     0,
                     0);
    }
    else
    if (m_is_cancelled.load() )
    {
        set_progress(InstallPhase::CANCELLED,
                     "Cancelled",
                     0,
                     0);
    }
    else
    {
        set_progress(InstallPhase::FAILED,
                     error,
                     0,
                     0);
    }

    m_is_busy = false;
}

void VR::AppInstaller::AppInstaller::set_progress(const InstallPhase& in_phase,
                                                  const std::string&  in_message,
                                                  const uint64_t&     in_n_bytes_done,
                                                  const uint64_t&     in_n_bytes_total)
{
    std::lock_guard<std::mutex> lock(m_state.mutex);

    m_state.progress.message       = in_message;
    m_state.progress.n_bytes_done  = in_n_bytes_done;
    m_state.progress.n_bytes_total = in_n_bytes_total;
    m_state.progress.phase         = in_phase;
}

bool VR::AppInstaller::AppInstaller::start(const InstallSpec& in_spec,
                                           std::string*       out_error_ptr)
{
    bool        is_hash_valid = false;
    std::string parent;
    bool        result        = false;

    if (m_is_busy.load() )
    {
        *out_error_ptr = "An application is being installed already.";

        goto end;
    }

    is_hash_valid = is_sha256_hex(in_spec.sha256_hex);

    if (in_spec.install_directory.empty()     ||
        in_spec.url_vec.empty()               ||
        is_hash_valid == false                ||
        in_spec.n_unpacked_bytes_max == 0)
    {
        *out_error_ptr = "The application does not say where it comes from, or how to check it.";

        goto end;
    }

    if (FileSystem::is_directory(in_spec.install_directory) )
    {
        if (FileSystem::is_directory_empty(in_spec.install_directory) == false)
        {
            *out_error_ptr = "The folder " + in_spec.install_directory + " is there, but the application is not in it. Remove the folder, "
                             "and launch the application again.";

            goto end;
        }
    }

    /* Everything which is written is under the directory of the applications: when it cannot be, the user is told so, and nothing is
     * written anywhere else.
     */
    parent = in_spec.install_directory.substr(0,
                                              in_spec.install_directory.find_last_of("\\/") );

    if (FileSystem::create_directories(parent,
                                       out_error_ptr) == false)
    {
        *out_error_ptr = "Demo VR Player cannot write to " + parent + ", so the application cannot be downloaded there.\n\nMove the folder "
                         "of Demo VR Player to a place which you can write to, and start it again.";

        goto end;
    }

    if (FileSystem::test_write(parent,
                               out_error_ptr) == false)
    {
        *out_error_ptr = "Demo VR Player cannot write to " + parent + ", so the application cannot be downloaded there.\n\nMove the folder "
                         "of Demo VR Player to a place which you can write to, and start it again.";

        goto end;
    }

    if (m_thread.joinable() )
    {
        m_thread.join();
    }

    m_is_cancelled = false;
    m_is_busy      = true;

    set_progress(InstallPhase::DOWNLOADING,
                 "Starting",
                 0,
                 in_spec.n_archive_bytes);

    {
        const InstallSpec spec     = in_spec;
        const auto        run_func = [this, spec]()
        {
            run_install(spec);
        };

        m_thread = std::thread(run_func);
    }

    result = true;

end:
    return result;
}

std::string VR::AppInstaller::AppInstaller::get_host(const std::string& in_url)
{
    const size_t n_scheme = in_url.find("://");
    const size_t n_start  = (n_scheme == std::string::npos) ? 0
                                                            : n_scheme + 3;
    const size_t n_end    = in_url.find_first_of("/:?",
                                                 n_start);

    return in_url.substr(n_start,
                         (n_end == std::string::npos) ? std::string::npos
                                                      : n_end - n_start);
}

bool VR::AppInstaller::AppInstaller::is_sha256_hex(const std::string& in_text)
{
    return (in_text.size() == 64                                                 &&
            in_text.find_first_not_of("0123456789abcdef") == std::string::npos);
}
