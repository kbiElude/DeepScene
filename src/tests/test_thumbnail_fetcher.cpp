/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Tests for the fetcher of the pictures of the applications: a picture is kept under its name once it is whole, one that is kept is not
 * downloaded again, a picture that cannot be had leaves nothing behind, and the fetcher says when each job is over.
 */
#include <gtest/gtest.h>
#include "temp_directory.h"
#include "utils/files/directory_operations.h"
#include "utils/files/i_downloader.h"
#include "vr_player/app_thumbnails/thumbnail_fetcher.h"
#include <Windows.h>
#include <chrono>
#include <fstream>
#include <map>
#include <thread>

/* Answers an address with the bytes that the test has set for it, or fails. */
class FakePictureDownloader : public Utils::Files::IDownloader
{
public:
    FakePictureDownloader()
        :m_n_calls(0)
    {
        /* Nothing to do. */
    }

    bool download(const std::string&                    in_url,
                  const std::string&                    in_file_path,
                  const std::atomic<bool>&              in_is_cancelled,
                  const Utils::Files::ProgressCallback& in_progress_func,
                  std::string*                          out_error_ptr) override
    {
        const auto answer_iterator = m_bytes_map.find(in_url);
        bool       result          = false;

        (void) in_is_cancelled;

        ++m_n_calls;

        if (answer_iterator == m_bytes_map.end() )
        {
            *out_error_ptr = "no answer";

            goto end;
        }

        {
            std::ofstream output_file(in_file_path,
                                      std::ios::binary);

            output_file.write(answer_iterator->second.data(),
                              static_cast<std::streamsize>(answer_iterator->second.size() ) );
        }

        in_progress_func(answer_iterator->second.size(),
                         answer_iterator->second.size() );

        result = true;

    end:
        return result;
    }

    std::map<std::string, std::string> m_bytes_map;
    std::atomic<int>                   m_n_calls;
};

class ThumbnailFetcherTest : public ::testing::Test
{
protected:
    /* Waits until the fetcher has finished the number of jobs, for at most ten seconds. */
    bool wait_for(VRPlayer::AppThumbnails::ThumbnailFetcher& in_fetcher,
                  const uint64_t&                            in_n_jobs)
    {
        const auto end_time = std::chrono::steady_clock::now() + std::chrono::seconds(10);

        while (in_fetcher.get_n_finished() < in_n_jobs &&
               std::chrono::steady_clock::now() < end_time)
        {
            ::Sleep(5);
        }

        return (in_fetcher.get_n_finished() >= in_n_jobs);
    }

    FakePictureDownloader m_downloader;
    Tests::TempDirectory  m_directory;
};

TEST_F(ThumbnailFetcherTest, APictureIsKeptUnderItsNameOnceItIsWholeAndNothingElseIsLeftBehind)
{
    const std::string file_path = m_directory.get_path() + "\\thumbnails\\one.jpg";

    m_downloader.m_bytes_map["https://pictures.example/one.jpg"] = "picture";

    VRPlayer::AppThumbnails::ThumbnailFetcher fetcher(&m_downloader);

    fetcher.start({{file_path,
                    "https://pictures.example/one.jpg"}});

    ASSERT_TRUE(wait_for(fetcher,
                         1) );

    EXPECT_TRUE (Utils::Files::DirectoryOperations::is_file(file_path) );
    EXPECT_FALSE(Utils::Files::DirectoryOperations::is_file(file_path + ".part") );
}

TEST_F(ThumbnailFetcherTest, APictureThatIsKeptIsNotDownloadedAgain)
{
    const std::string file_path = m_directory.get_path() + "\\kept.jpg";

    {
        std::ofstream output_file(file_path,
                                  std::ios::binary);

        output_file << "kept";
    }

    m_downloader.m_bytes_map["https://pictures.example/kept.jpg"] = "new";

    VRPlayer::AppThumbnails::ThumbnailFetcher fetcher(&m_downloader);

    fetcher.start({{file_path,
                    "https://pictures.example/kept.jpg"}});

    ASSERT_TRUE(wait_for(fetcher,
                         1) );

    EXPECT_EQ(m_downloader.m_n_calls.load(),
              0);
}

TEST_F(ThumbnailFetcherTest, APictureThatCannotBeHadLeavesNothingAndTheOthersAreFetched)
{
    const std::string missing_path = m_directory.get_path() + "\\missing.jpg";
    const std::string second_path  = m_directory.get_path() + "\\second.jpg";

    m_downloader.m_bytes_map["https://pictures.example/second.jpg"] = "second";

    VRPlayer::AppThumbnails::ThumbnailFetcher fetcher(&m_downloader);

    fetcher.start({{missing_path,
                    "https://pictures.example/missing.jpg"},
                   {second_path,
                    "https://pictures.example/second.jpg"}});

    ASSERT_TRUE(wait_for(fetcher,
                         2) );

    EXPECT_FALSE(Utils::Files::DirectoryOperations::is_file(missing_path) );
    EXPECT_FALSE(Utils::Files::DirectoryOperations::is_file(missing_path + ".part") );
    EXPECT_TRUE (Utils::Files::DirectoryOperations::is_file(second_path) );
    EXPECT_EQ   (fetcher.get_n_finished(),
                 2u);
}
