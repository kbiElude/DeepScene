/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Tests for AppInstaller, with a downloader which hands over an archive from the test: the application is put in its directory, and
 * what is wrong (another file than the expected, an archive which writes outside, mirrors which fail, a directory which cannot be
 * written to, a cancel) leaves nothing behind.
 */
#include <gtest/gtest.h>
#include "VR/AppInstaller/app_installer.h"
#include "VR/AppInstaller/file_system.h"
#include "VR/AppInstaller/sha256.h"
#include "temp_directory.h"
#include "zip_builder.h"
#include <Windows.h>
#include <chrono>
#include <fstream>
#include <map>
#include <sstream>

/* Answers each address with what the test has set for it: bytes, or a failure; a "stall" waits for the cancel. */
class FakeDownloader : public VR::AppInstaller::IDownloader
{
public:
    FakeDownloader()
        :m_n_calls(0)
    {
        /* Nothing to do. */
    }

    bool download(const std::string&                    in_url,
                  const std::string&                    in_file_path,
                  const std::atomic<bool>&              in_is_cancelled,
                  const VR::AppInstaller::ProgressCallback& in_progress_func,
                  std::string*                          out_error_ptr) override
    {
        const auto iterator = m_bytes_map.find(in_url);
        bool       result   = false;

        ++m_n_calls;

        if (iterator == m_bytes_map.end() )
        {
            *out_error_ptr = "no answer";

            goto end;
        }

        if (iterator->second == "stall")
        {
            while (in_is_cancelled.load() == false)
            {
                ::Sleep(10);
            }

            *out_error_ptr = "Cancelled";

            goto end;
        }

        {
            std::ofstream file(in_file_path,
                               std::ios::binary);

            file.write(iterator->second.data(),
                       static_cast<std::streamsize>(iterator->second.size() ) );
        }

        in_progress_func(iterator->second.size(),
                         iterator->second.size() );

        result = true;

    end:
        return result;
    }

    std::map<std::string, std::string> m_bytes_map;
    int                                m_n_calls;
};

class AppInstallerTest : public ::testing::Test
{
protected:
    AppInstallerTest()
        :m_installer(&m_downloader)
    {
        m_zip.add_file("app.exe",
                       "program");
        m_zip.add_file("data/readme.txt",
                       "read me");

        m_downloader.m_bytes_map["https://one.example/app.zip"] = m_zip.get_bytes();

        m_spec.install_directory     = m_directory.get_path() + "\\apps\\app";
        m_spec.n_archive_bytes       = m_zip.get_bytes().size();
        m_spec.n_unpacked_bytes_max  = 100000;
        m_spec.page_url              = "https://files.example/view/app.zip";
        m_spec.url_vec.push_back("https://one.example/app.zip");

        VR::AppInstaller::Sha256::hash_bytes(m_zip.get_bytes().data(),
                                             m_zip.get_bytes().size(),
                                            &m_spec.sha256_hex);
    }

    /* Waits for the end of the installation: DONE, FAILED or CANCELLED. */
    VR::AppInstaller::InstallProgress wait()
    {
        const auto start = std::chrono::steady_clock::now();

        while (m_installer.is_busy() &&
               std::chrono::steady_clock::now() - start < std::chrono::seconds(20) )
        {
            ::Sleep(5);
        }

        return m_installer.get_progress();
    }

    std::string get_text(const std::string& in_path) const
    {
        std::ifstream      file(in_path,
                                std::ios::binary);
        std::ostringstream text;

        text << file.rdbuf();

        return text.str();
    }

    Tests::TempDirectory                m_directory;
    FakeDownloader                      m_downloader;
    VR::AppInstaller::AppInstaller      m_installer;
    VR::AppInstaller::InstallSpec       m_spec;
    Tests::ZipBuilder                   m_zip;
};

TEST_F(AppInstallerTest, TheApplicationIsPutInItsDirectoryAndNothingIsLeftNextToIt)
{
    std::string error;

    ASSERT_TRUE(m_installer.start(m_spec,
                                 &error) ) << error;

    const VR::AppInstaller::InstallProgress progress = wait();

    ASSERT_EQ(progress.phase,
              VR::AppInstaller::InstallPhase::DONE) << progress.message;

    EXPECT_EQ(get_text(m_spec.install_directory + "\\app.exe"),
              "program");
    EXPECT_EQ(get_text(m_spec.install_directory + "\\data\\readme.txt"),
              "read me");
    EXPECT_FALSE(VR::AppInstaller::FileSystem::is_directory(m_spec.install_directory + ".download") );
}

TEST_F(AppInstallerTest, AFileWhichIsNotTheExpectedOneIsNotUsed)
{
    std::string error;

    m_spec.sha256_hex = std::string(64,
                                    'a');

    ASSERT_TRUE(m_installer.start(m_spec,
                                 &error) ) << error;

    const VR::AppInstaller::InstallProgress progress = wait();

    EXPECT_EQ(progress.phase,
              VR::AppInstaller::InstallPhase::FAILED);
    EXPECT_NE(progress.message.find("not the file which was expected"),
              std::string::npos) << progress.message;
    EXPECT_FALSE(VR::AppInstaller::FileSystem::is_directory(m_spec.install_directory) );
    EXPECT_FALSE(VR::AppInstaller::FileSystem::is_directory(m_spec.install_directory + ".download") );
}

TEST_F(AppInstallerTest, TheNextAddressIsTriedWhenOneFailsAndAllFailingSaysSoWithThePage)
{
    std::string error;

    m_spec.url_vec.insert(m_spec.url_vec.begin(),
                          "https://broken.example/app.zip");

    ASSERT_TRUE(m_installer.start(m_spec,
                                 &error) ) << error;

    EXPECT_EQ(wait().phase,
              VR::AppInstaller::InstallPhase::DONE);
    EXPECT_EQ(m_downloader.m_n_calls,
              2);

    m_spec.install_directory = m_directory.get_path() + "\\apps\\other";
    m_spec.url_vec.clear();
    m_spec.url_vec.push_back("https://broken.example/one.zip");
    m_spec.url_vec.push_back("https://broken.example/two.zip");

    ASSERT_TRUE(m_installer.start(m_spec,
                                 &error) ) << error;

    const VR::AppInstaller::InstallProgress progress = wait();

    EXPECT_EQ(progress.phase,
              VR::AppInstaller::InstallPhase::FAILED);
    EXPECT_NE(progress.message.find("broken.example"),
              std::string::npos) << progress.message;
    EXPECT_NE(progress.message.find(m_spec.page_url),
              std::string::npos) << progress.message;
}

TEST_F(AppInstallerTest, AnArchiveWhichWritesOutsideItsDirectoryIsRefusedAndWritesNothing)
{
    Tests::ZipBuilder evil;
    std::string       error;

    evil.add_file("app.exe",
                  "program");
    evil.add_file("../../escaped.txt",
                  "evil");

    m_downloader.m_bytes_map["https://one.example/app.zip"] = evil.get_bytes();

    VR::AppInstaller::Sha256::hash_bytes(evil.get_bytes().data(),
                                         evil.get_bytes().size(),
                                        &m_spec.sha256_hex);

    ASSERT_TRUE(m_installer.start(m_spec,
                                 &error) ) << error;

    const VR::AppInstaller::InstallProgress progress = wait();

    EXPECT_EQ(progress.phase,
              VR::AppInstaller::InstallPhase::FAILED);
    EXPECT_FALSE(VR::AppInstaller::FileSystem::is_file(m_directory.get_path() + "\\escaped.txt") );
    EXPECT_FALSE(VR::AppInstaller::FileSystem::is_file(m_directory.get_path() + "\\apps\\escaped.txt") );
    EXPECT_FALSE(VR::AppInstaller::FileSystem::is_directory(m_spec.install_directory) );
    EXPECT_FALSE(VR::AppInstaller::FileSystem::is_directory(m_spec.install_directory + ".download") );
}

TEST_F(AppInstallerTest, ADirectoryWhichCannotBeWrittenToIsReportedAtTheStartAndNothingIsDone)
{
    std::string error;

    {
        std::ofstream file(m_directory.get_path() + "\\blocker");

        file << "x";
    }

    m_spec.install_directory = m_directory.get_path() + "\\blocker\\apps\\app";

    EXPECT_FALSE(m_installer.start(m_spec,
                                  &error) );
    EXPECT_NE   (error.find("cannot write"),
                 std::string::npos) << error;
    EXPECT_FALSE(m_installer.is_busy() );
    EXPECT_EQ   (m_downloader.m_n_calls,
                 0);
}

TEST_F(AppInstallerTest, AnApplicationWhoseDirectoryIsTakenOrWhichIsNotWellDescribedIsNotStarted)
{
    std::string error;

    ASSERT_TRUE(VR::AppInstaller::FileSystem::create_directories(m_spec.install_directory,
                                                                &error) );
    EXPECT_TRUE(m_installer.start(m_spec,
                                 &error) ) << "an empty directory can be used";
    EXPECT_EQ  (wait().phase,
                VR::AppInstaller::InstallPhase::DONE);

    EXPECT_FALSE(m_installer.start(m_spec,
                                  &error) ) << "the application is there";
    EXPECT_NE   (error.find("is there"),
                 std::string::npos) << error;

    m_spec.install_directory = m_directory.get_path() + "\\apps\\second";
    m_spec.sha256_hex        = "not a hash";

    EXPECT_FALSE(m_installer.start(m_spec,
                                  &error) );

    m_spec.sha256_hex = std::string(64,
                                    'b');
    m_spec.url_vec.clear();

    EXPECT_FALSE(m_installer.start(m_spec,
                                  &error) );
}

TEST_F(AppInstallerTest, ACancelEndsTheInstallationAndLeavesNothing)
{
    std::string error;

    m_downloader.m_bytes_map["https://one.example/app.zip"] = "stall";

    ASSERT_TRUE(m_installer.start(m_spec,
                                 &error) ) << error;

    EXPECT_TRUE(m_installer.is_busy() );

    m_installer.cancel();

    EXPECT_EQ(wait().phase,
              VR::AppInstaller::InstallPhase::CANCELLED);
    EXPECT_FALSE(VR::AppInstaller::FileSystem::is_directory(m_spec.install_directory) );
    EXPECT_FALSE(VR::AppInstaller::FileSystem::is_directory(m_spec.install_directory + ".download") );
}

TEST_F(AppInstallerTest, OneInstallationIsRunAtATime)
{
    std::string error;

    m_downloader.m_bytes_map["https://one.example/app.zip"] = "stall";

    ASSERT_TRUE(m_installer.start(m_spec,
                                 &error) ) << error;

    EXPECT_FALSE(m_installer.start(m_spec,
                                  &error) );
    EXPECT_NE   (error.find("being installed"),
                 std::string::npos) << error;

    m_installer.cancel();
    wait();
}
