/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Tests for ZipExtractor: what is unpacked, and what is refused (names which would put a file outside the directory, entries which
 * are too big or the same, a zip file which is damaged, an unpacking which is cancelled).
 */
#include <gtest/gtest.h>
#include "VR/AppInstaller/file_system.h"
#include "VR/AppInstaller/zip_extractor.h"
#include "temp_directory.h"
#include "zip_builder.h"
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

class ZipExtractorTest : public ::testing::Test
{
protected:
    bool extract(const Tests::ZipBuilder& in_zip,
                 const uint64_t&          in_n_unpacked_bytes_max = 1000000)
    {
        const std::string       archive_path = m_directory.get_path() + "\\test.zip";
        const std::atomic<bool> is_cancelled(m_is_cancelled);

        EXPECT_TRUE(in_zip.write_file(archive_path) );

        m_error.clear();

        return VR::AppInstaller::ZipExtractor::extract(archive_path,
                                                       m_directory.get_path() + "\\out",
                                                       in_n_unpacked_bytes_max,
                                                       is_cancelled,
                                                       VR::AppInstaller::ProgressCallback(),
                                                      &m_error);
    }

    Tests::TempDirectory m_directory;
    std::string          m_error;
    bool                 m_is_cancelled = false;
};

TEST_F(ZipExtractorTest, TheEntriesAreUnpackedWithTheirDirectories)
{
    Tests::ZipBuilder zip_builder;

    zip_builder.add_file("readme.txt",
                         "hello");
    zip_builder.add_file("data/",
                         "");
    zip_builder.add_file("data/deep/file.bin",
                         std::string("a\0b",
                                     3) );

    ASSERT_TRUE(extract(zip_builder) ) << m_error;

    EXPECT_EQ(get_text(m_directory.get_path() + "\\out\\readme.txt"),
              "hello");
    EXPECT_EQ(get_text(m_directory.get_path() + "\\out\\data\\deep\\file.bin"),
              std::string("a\0b",
                          3) );
    EXPECT_TRUE(VR::AppInstaller::FileSystem::is_directory(m_directory.get_path() + "\\out\\data") );
}

TEST_F(ZipExtractorTest, ANameWhichLeavesTheDirectoryIsRefusedAndNothingIsWritten)
{
    const std::string NAME_ARRAY[] = {"../evil.txt", "a/../../evil.txt", "..\\evil.txt", "/evil.txt", "\\evil.txt", "C:/evil.txt", "C:evil.txt",
                                      "a:b", "con", "dir/NUL.txt", "dir/aux", "trailing.", "space /x", "./x", "a//b"};

    for (const std::string& entry_name : NAME_ARRAY)
    {
        Tests::ZipBuilder zip_builder;

        zip_builder.add_file("fine.txt",
                             "fine");
        zip_builder.add_file(entry_name,
                             "evil");

        EXPECT_FALSE(extract(zip_builder) ) << entry_name;
        EXPECT_NE   (m_error.find("not allowed"),
                     std::string::npos) << entry_name << ": " << m_error;
        EXPECT_FALSE(VR::AppInstaller::FileSystem::is_file(m_directory.get_path() + "\\out\\fine.txt") ) << "what is before is not written either";
        EXPECT_FALSE(VR::AppInstaller::FileSystem::is_file(m_directory.get_path() + "\\evil.txt") );
    }
}

TEST_F(ZipExtractorTest, TheNamesWhichStayInsideAreAccepted)
{
    EXPECT_TRUE (VR::AppInstaller::ZipExtractor::is_name_safe("a.txt") );
    EXPECT_TRUE (VR::AppInstaller::ZipExtractor::is_name_safe("dir/sub/a.txt") );
    EXPECT_TRUE (VR::AppInstaller::ZipExtractor::is_name_safe("dir/") );
    EXPECT_TRUE (VR::AppInstaller::ZipExtractor::is_name_safe("a.b.c") );
    EXPECT_TRUE (VR::AppInstaller::ZipExtractor::is_name_safe("console.txt") );
    EXPECT_FALSE(VR::AppInstaller::ZipExtractor::is_name_safe("") );
}

TEST_F(ZipExtractorTest, MoreThanTheLimitIsRefusedBeforeAnythingIsWritten)
{
    Tests::ZipBuilder zip_builder;

    zip_builder.add_file("a.bin",
                         std::string(600,
                                     'a') );
    zip_builder.add_file("b.bin",
                         std::string(600,
                                     'b') );

    EXPECT_FALSE(extract(zip_builder,
                         1000) );
    EXPECT_NE   (m_error.find("more than"),
                 std::string::npos) << m_error;
    EXPECT_FALSE(VR::AppInstaller::FileSystem::is_file(m_directory.get_path() + "\\out\\a.bin") );
}

TEST_F(ZipExtractorTest, TwoNamesWhichAreTheSameForTheSystemAreRefused)
{
    Tests::ZipBuilder zip_builder;

    zip_builder.add_file("File.txt",
                         "one");
    zip_builder.add_file("file.TXT",
                         "two");

    EXPECT_FALSE(extract(zip_builder) );
    EXPECT_NE   (m_error.find("twice"),
                 std::string::npos) << m_error;
}

TEST_F(ZipExtractorTest, ADamagedZipFileIsRefused)
{
    Tests::ZipBuilder zip_builder;
    std::string       zip_file_bytes;

    zip_builder.add_file("a.txt",
                         std::string(5000,
                                     'x') );

    zip_file_bytes = zip_builder.get_bytes();

    ASSERT_FALSE(zip_file_bytes.empty() );

    /* A byte in the middle of what is compressed. */
    zip_file_bytes.at(40) = static_cast<char>(zip_file_bytes.at(40) ^ 0x55);

    {
        std::ofstream output_file(m_directory.get_path() + "\\damaged.zip",
                                  std::ios::binary);

        output_file.write(zip_file_bytes.data(),
                          static_cast<std::streamsize>(zip_file_bytes.size() ) );
    }

    const std::atomic<bool> is_cancelled(false);

    EXPECT_FALSE(VR::AppInstaller::ZipExtractor::extract(m_directory.get_path() + "\\damaged.zip",
                                                         m_directory.get_path() + "\\out",
                                                         1000000,
                                                         is_cancelled,
                                                         VR::AppInstaller::ProgressCallback(),
                                                        &m_error) );
    EXPECT_FALSE(m_error.empty() );

    EXPECT_FALSE(VR::AppInstaller::ZipExtractor::extract(m_directory.get_path() + "\\not_there.zip",
                                                         m_directory.get_path() + "\\out",
                                                         1000000,
                                                         is_cancelled,
                                                         VR::AppInstaller::ProgressCallback(),
                                                        &m_error) );
}

TEST_F(ZipExtractorTest, TheProgressReachesTheSizeOfWhatIsUnpacked)
{
    const std::atomic<bool> is_cancelled(false);
    uint64_t                n_bytes_done  = 0;
    uint64_t                n_bytes_total = 0;
    Tests::ZipBuilder       zip_builder;

    zip_builder.add_file("a.bin",
                         std::string(3000,
                                     'a') );
    zip_builder.add_file("b.bin",
                         std::string(2000,
                                     'b') );

    ASSERT_TRUE(zip_builder.write_file(m_directory.get_path() + "\\p.zip") );

    ASSERT_TRUE(VR::AppInstaller::ZipExtractor::extract(m_directory.get_path() + "\\p.zip",
                                                        m_directory.get_path() + "\\out",
                                                        1000000,
                                                        is_cancelled,
                                                        [&](const uint64_t& in_n_bytes_done, const uint64_t& in_n_bytes_total)
                                                        {
                                                            n_bytes_done  = in_n_bytes_done;
                                                            n_bytes_total = in_n_bytes_total;
                                                        },
                                                       &m_error) ) << m_error;

    EXPECT_EQ(n_bytes_done,
              5000u);
    EXPECT_EQ(n_bytes_total,
              5000u);
}

TEST_F(ZipExtractorTest, AnUnpackingWhichIsCancelledStops)
{
    Tests::ZipBuilder zip_builder;

    m_is_cancelled = true;

    zip_builder.add_file("a.bin",
                         std::string(3000,
                                     'a') );

    EXPECT_FALSE(extract(zip_builder) );
    EXPECT_EQ   (m_error,
                 "Cancelled");
}
