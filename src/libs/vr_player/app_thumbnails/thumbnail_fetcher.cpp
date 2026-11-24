/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "utils/files/directory_operations.h"
#include "vr_player/app_thumbnails/thumbnail_fetcher.h"
#include <Windows.h>

VRPlayer::AppThumbnails::ThumbnailFetcher::ThumbnailFetcher(Utils::Files::IDownloader* in_downloader_ptr)
    :m_downloader_ptr(in_downloader_ptr),
     m_is_cancelled  (false),
     m_n_finished    (0)
{
    /* Nothing to do. */
}

VRPlayer::AppThumbnails::ThumbnailFetcher::~ThumbnailFetcher()
{
    m_is_cancelled = true;

    if (m_thread.joinable() )
    {
        m_thread.join();
    }
}

uint64_t VRPlayer::AppThumbnails::ThumbnailFetcher::get_n_finished() const
{
    return m_n_finished.load();
}

void VRPlayer::AppThumbnails::ThumbnailFetcher::run(const std::vector<ThumbnailJob>& in_job_vec)
{
    const auto no_progress_func = [](const uint64_t& in_n_bytes_done,
                                     const uint64_t& in_n_bytes_total)
    {
        (void) in_n_bytes_done;
        (void) in_n_bytes_total;
    };

    for (const ThumbnailJob& job : in_job_vec)
    {
        if (m_is_cancelled.load() )
        {
            break;
        }

        if (Utils::Files::DirectoryOperations::is_file(job.file_path) == false)
        {
            const size_t      n_separator = job.file_path.find_last_of("\\/");
            const std::string part_path   = job.file_path + ".part";
            std::string       error;

            if (n_separator != std::string::npos)
            {
                Utils::Files::DirectoryOperations::create_directories(job.file_path.substr(0,
                                                                                           n_separator),
                                                                      &error);
            }

            if (m_downloader_ptr->download(job.url,
                                           part_path,
                                           m_is_cancelled,
                                           no_progress_func,
                                          &error) )
            {
                ::MoveFileExA(part_path.c_str(),
                              job.file_path.c_str(),
                              MOVEFILE_REPLACE_EXISTING);
            }
            else
            {
                ::DeleteFileA(part_path.c_str() );
            }
        }

        ++m_n_finished;
    }
}

void VRPlayer::AppThumbnails::ThumbnailFetcher::start(const std::vector<ThumbnailJob>& in_job_vec)
{
    m_thread = std::thread(&ThumbnailFetcher::run,
                            this,
                            in_job_vec);
}
