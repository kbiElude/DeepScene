/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Tests for HttpDownloader, against a server on this computer: the bytes and the progress, a redirect, an answer which is not 200, a
 * download which ends early, the refusal of an address which is not https, and a cancel while the server says nothing. What the
 * servers of scene.org do is not tested here.
 */
#include <gtest/gtest.h>
#include "vr/app_installer/file_system.h"
#include "vr/app_installer/http_downloader.h"
#include "local_http_server.h"
#include "temp_directory.h"
#include <Windows.h>
#include <chrono>
#include <fstream>
#include <sstream>

static std::string get_text(const std::string& in_path)
{
    std::ostringstream file_text;
    std::ifstream      input_file(in_path,
                                  std::ios::binary);

    file_text << input_file.rdbuf();

    return file_text.str();
}

class HttpDownloaderTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        ASSERT_TRUE(m_server.start() );
    }

    Tests::LocalHttpServer::Answer get_answer(const std::string& in_body) const
    {
        Tests::LocalHttpServer::Answer result;

        result.response_body = in_body;

        return result;
    }

    HttpDownloaderTest()
        :m_is_cancelled(false)
    {
        /* Nothing to do. */
    }

    Tests::TempDirectory   m_directory;
    std::string            m_error;
    std::atomic<bool>      m_is_cancelled;
    Tests::LocalHttpServer m_server;
};

TEST_F(HttpDownloaderTest, TheBytesAreWrittenAndTheProgressReachesTheLength)
{
    VR::AppInstaller::HttpDownloader downloader(true /* in_is_http_allowed */);
    bool                             is_monotonic  = true;
    uint64_t                         n_bytes_done  = 0;
    uint64_t                         n_bytes_total = 0;
    uint64_t                         n_previous    = 0;
    const std::string                response_body(300000,
                                                   'q');

    m_server.set_answer("/file",
                        get_answer(response_body) );

    ASSERT_TRUE(downloader.download(m_server.get_url("/file"),
                                    m_directory.get_path() + "\\file.bin",
                                    m_is_cancelled,
                                    [&](const uint64_t& in_n_bytes_done, const uint64_t& in_n_bytes_total)
                                    {
                                        is_monotonic  = is_monotonic && (in_n_bytes_done >= n_previous);
                                        n_previous    = in_n_bytes_done;
                                        n_bytes_done  = in_n_bytes_done;
                                        n_bytes_total = in_n_bytes_total;
                                    },
                                   &m_error) ) << m_error;

    EXPECT_EQ(get_text(m_directory.get_path() + "\\file.bin"),
              response_body);
    EXPECT_EQ(n_bytes_done,
              response_body.size() );
    EXPECT_EQ(n_bytes_total,
              response_body.size() );
    EXPECT_TRUE(is_monotonic);
}

TEST_F(HttpDownloaderTest, ARedirectIsFollowed)
{
    VR::AppInstaller::HttpDownloader downloader(true /* in_is_http_allowed */);
    Tests::LocalHttpServer::Answer   redirect_answer;

    redirect_answer.http_status_code   = 302;
    redirect_answer.redirect_location = m_server.get_url("/file");

    m_server.set_answer("/file",
                        get_answer("the file") );
    m_server.set_answer("/old",
                        redirect_answer);

    ASSERT_TRUE(downloader.download(m_server.get_url("/old"),
                                    m_directory.get_path() + "\\file.bin",
                                    m_is_cancelled,
                                    VR::AppInstaller::ProgressCallback(),
                                   &m_error) ) << m_error;
    EXPECT_EQ  (get_text(m_directory.get_path() + "\\file.bin"),
                "the file");
}

TEST_F(HttpDownloaderTest, AnAnswerWhichIsNotOkFailsTheDownload)
{
    VR::AppInstaller::HttpDownloader downloader(true /* in_is_http_allowed */);

    EXPECT_FALSE(downloader.download(m_server.get_url("/missing"),
                                     m_directory.get_path() + "\\file.bin",
                                     m_is_cancelled,
                                     VR::AppInstaller::ProgressCallback(),
                                    &m_error) );
    EXPECT_NE   (m_error.find("404"),
                 std::string::npos) << m_error;
}

TEST_F(HttpDownloaderTest, ADownloadWhichEndsBeforeItsLengthFails)
{
    VR::AppInstaller::HttpDownloader downloader(true /* in_is_http_allowed */);
    Tests::LocalHttpServer::Answer   short_answer = get_answer("short");

    short_answer.n_declared_bytes = 1000;

    m_server.set_answer("/short",
                        short_answer);

    EXPECT_FALSE(downloader.download(m_server.get_url("/short"),
                                     m_directory.get_path() + "\\file.bin",
                                     m_is_cancelled,
                                     VR::AppInstaller::ProgressCallback(),
                                    &m_error) );
    EXPECT_FALSE(m_error.empty() );
}

TEST_F(HttpDownloaderTest, AnAddressWhichIsNotHttpsIsRefusedUnlessTheTestAllowsIt)
{
    VR::AppInstaller::HttpDownloader downloader(false /* in_is_http_allowed */);

    m_server.set_answer("/file",
                        get_answer("the file") );

    EXPECT_FALSE(downloader.download(m_server.get_url("/file"),
                                     m_directory.get_path() + "\\file.bin",
                                     m_is_cancelled,
                                     VR::AppInstaller::ProgressCallback(),
                                    &m_error) );
    EXPECT_NE   (m_error.find("https"),
                 std::string::npos) << m_error;
    EXPECT_FALSE(VR::AppInstaller::FileSystem::is_file(m_directory.get_path() + "\\file.bin") );

    EXPECT_FALSE(downloader.download("not an address",
                                     m_directory.get_path() + "\\file.bin",
                                     m_is_cancelled,
                                     VR::AppInstaller::ProgressCallback(),
                                    &m_error) );
}

TEST_F(HttpDownloaderTest, ACancelWhileTheServerSaysNothingEndsTheDownloadAtTheNextTimeout)
{
    constexpr int                    RECEIVE_TIMEOUT_MS = 400;

    std::thread                      cancel_thread;
    VR::AppInstaller::HttpDownloader downloader(true /* in_is_http_allowed */,
                                                RECEIVE_TIMEOUT_MS);
    Tests::LocalHttpServer::Answer   stalled_answer = get_answer(std::string(10000,
                                                                             'z') );
    const auto                       start_time     = std::chrono::steady_clock::now();

    stalled_answer.should_stall = true;

    m_server.set_answer("/stall",
                        stalled_answer);

    cancel_thread = std::thread([this]()
                                {
                                    ::Sleep(200);
    
                                    m_is_cancelled = true;
                                });

    EXPECT_FALSE(downloader.download(m_server.get_url("/stall"),
                                     m_directory.get_path() + "\\file.bin",
                                     m_is_cancelled,
                                     VR::AppInstaller::ProgressCallback(),
                                    &m_error) );

    cancel_thread.join();

    EXPECT_EQ(m_error,
              "Cancelled");
    EXPECT_LT(std::chrono::steady_clock::now() - start_time,
              std::chrono::seconds(10) );
}
