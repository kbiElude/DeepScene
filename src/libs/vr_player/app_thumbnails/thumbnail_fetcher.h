/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Downloads the pictures of the applications, one after the other, on a thread of its own, so that the window never waits for the network. A
 * picture that is kept already is not downloaded again. A picture that cannot be had is skipped without a word: the card has no picture then.
 * A file is written under the name of the picture plus ".part" and renamed when it is whole, so a file that is there is never half of one.
 */
#ifndef APPTHUMBNAILS_THUMBNAIL_FETCHER_H
#define APPTHUMBNAILS_THUMBNAIL_FETCHER_H

#include "utils/files/i_downloader.h"
#include "vr_player/app_thumbnails/types.h"
#include <atomic>
#include <stdint.h>
#include <thread>
#include <vector>

namespace VRPlayer
{
    namespace AppThumbnails
    {
        class ThumbnailFetcher
        {
        public:
            /* The downloader is not owned and must outlive this object. It is used from the thread of the fetcher only, so it must not be shared. */
            explicit ThumbnailFetcher(Utils::Files::IDownloader* in_downloader_ptr);

            /* Stops the download that is running, if any, and waits for it. */
            ~ThumbnailFetcher();

            ThumbnailFetcher(const ThumbnailFetcher&)            = delete;
            ThumbnailFetcher& operator=(const ThumbnailFetcher&) = delete;

            /* The number of jobs that are over, whether the picture was had or not. It grows by one for each job, and the window looks again
             * for the pictures when it changes.
             */
            uint64_t get_n_finished() const;

            /* Starts on the jobs. Call once. */
            void start(const std::vector<ThumbnailJob>& in_job_vec);

        private:
            void run(const std::vector<ThumbnailJob>& in_job_vec);

            Utils::Files::IDownloader* m_downloader_ptr;
            std::atomic<bool>          m_is_cancelled; /* not guarded: set by the destructor, read by the thread    */
            std::atomic<uint64_t>      m_n_finished;   /* not guarded: set by the thread, read by the window        */
            std::thread                m_thread;
        };
    }
}

#endif /* APPTHUMBNAILS_THUMBNAIL_FETCHER_H */
