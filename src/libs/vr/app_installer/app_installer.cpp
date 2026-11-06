/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "vr/app_installer/app_installer.h"
#include "vr/app_installer/file_system.h"
#include "vr/app_installer/sha256.h"
#include "vr/app_installer/zip_extractor.h"
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
        std::lock_guard<std::mutex> state_lock(m_shared_state.progress_mutex);

        result = m_shared_state.progress;
    }

    return result;
}

bool VR::AppInstaller::AppInstaller::is_busy() const
{
    return m_is_busy.load();
}

void VR::AppInstaller::AppInstaller::run_install(const InstallSpec& in_spec)
{
    std::string       archive_hash_hex;
    const std::string archive_path        = in_spec.install_directory + ".download\\archive.zip";
    uint64_t          directory_name_hash = 14695981039346656037ull;
    ProgressCallback  download_func;
    std::string       error;
    ProgressCallback  extract_func;
    bool              is_downloaded       = false;
    bool              is_installed        = false;
    DWORD             lock_error          = 0;
    HANDLE            lock_handle         = nullptr;
    std::string       lock_name;
    std::string       mirror_error_text;
    std::string       staging_directory   = in_spec.install_directory + ".download";
    std::string       unpacked_directory  = staging_directory + "\\unpacked";

    /* One installation of a directory at a time, whichever DeepScene it is started by. */
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
            directory_name_hash = (directory_name_hash ^ static_cast<unsigned char>(lower_directory.at(n_character) ) ) * 1099511628211ull;
        }

        lock_name = "Local\\DeepScene.Install." + std::to_string(directory_name_hash);
    }

    {
        const std::wstring wide_lock_name = FileSystem::to_wide(lock_name);

        lock_handle = ::CreateMutexW(nullptr,
                                     FALSE,
                                     wide_lock_name.c_str() );
    }

    lock_error = ::GetLastError();

    if (lock_handle == nullptr                ||
        lock_error  == ERROR_ALREADY_EXISTS)
    {
        error = "Another DeepScene is installing this application.";

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
        const std::string progress_text = "Downloading from " + get_host(in_spec.url_vec.at(n_url) );
        std::string       url_error;

        set_progress(InstallPhase::DOWNLOADING,
                     progress_text,
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

        mirror_error_text += "\n" + get_host(in_spec.url_vec.at(n_url) ) + ": " + url_error;
    }

    if (is_downloaded == false)
    {
        error = "The application could not be downloaded." + mirror_error_text;

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
                         &archive_hash_hex,
                         &error) == false)
    {
        goto end;
    }

    if (archive_hash_hex != in_spec.sha256_hex)
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

    {
        const std::wstring wide_install_directory  = FileSystem::to_wide(in_spec.install_directory);
        const std::wstring wide_unpacked_directory = FileSystem::to_wide(unpacked_directory);

        if (::MoveFileExW(wide_unpacked_directory.c_str(),
                          wide_install_directory.c_str(),
                          0) == FALSE)
        {
            error = "Cannot put the application in " + in_spec.install_directory + " (error " + std::to_string(::GetLastError() ) + ")";

            goto end;
        }
    }

    is_installed = true;

end:
    FileSystem::remove_tree(staging_directory);

    if (lock_handle != nullptr)
    {
        ::CloseHandle(lock_handle);
    }

    if (is_installed)
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
    std::lock_guard<std::mutex> state_lock(m_shared_state.progress_mutex);

    m_shared_state.progress.message       = in_message;
    m_shared_state.progress.n_bytes_done  = in_n_bytes_done;
    m_shared_state.progress.n_bytes_total = in_n_bytes_total;
    m_shared_state.progress.phase         = in_phase;
}

bool VR::AppInstaller::AppInstaller::start(const InstallSpec& in_spec,
                                           std::string*       out_error_ptr)
{
    bool        is_hash_valid = false;
    std::string parent_directory;
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
    {
        const size_t n_last_separator_char = in_spec.install_directory.find_last_of("\\/");

        parent_directory = in_spec.install_directory.substr(0,
                                                            n_last_separator_char);
    }

    if (FileSystem::create_directories(parent_directory,
                                       out_error_ptr) == false)
    {
        *out_error_ptr = "DeepScene cannot write to " + parent_directory + ", so the application cannot be downloaded there.\n\nMove the folder "
                         "of DeepScene to a place which you can write to, and start it again.";

        goto end;
    }

    if (FileSystem::test_write(parent_directory,
                               out_error_ptr) == false)
    {
        *out_error_ptr = "DeepScene cannot write to " + parent_directory + ", so the application cannot be downloaded there.\n\nMove the folder "
                         "of DeepScene to a place which you can write to, and start it again.";

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
        const InstallSpec spec_copy = in_spec;

        const auto run_func = [this, spec_copy]()
        {
            run_install(spec_copy);
        };

        m_thread = std::thread(run_func);
    }

    result = true;

end:
    return result;
}

std::string VR::AppInstaller::AppInstaller::get_host(const std::string& in_url)
{
    const size_t n_scheme_separator_character = in_url.find("://");

    const size_t n_host_start_character = (n_scheme_separator_character == std::string::npos) ? 0
                                                                                              : n_scheme_separator_character + 3;

    const size_t n_host_end_character = in_url.find_first_of("/:?",
                                                             n_host_start_character);

    return in_url.substr(n_host_start_character,
                         (n_host_end_character == std::string::npos) ? std::string::npos
                                                                     : n_host_end_character - n_host_start_character);
}

bool VR::AppInstaller::AppInstaller::is_sha256_hex(const std::string& in_text)
{
    return (in_text.size() == 64                                                 &&
            in_text.find_first_not_of("0123456789abcdef") == std::string::npos);
}
