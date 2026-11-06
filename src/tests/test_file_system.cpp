/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Tests for FileSystem: directories which are made and removed, and the test of whether a directory can be written to.
 */
#include <gtest/gtest.h>
#include "utils/files/file_system.h"
#include "temp_directory.h"
#include <fstream>

TEST(FileSystemTest, ADirectoryIsMadeWithTheOnesAboveItAndRemovedWithWhatIsInIt)
{
    std::string                error;
    const Tests::TempDirectory temp_directory;

    const std::string deep_directory_path = temp_directory.get_path() + "\\a\\b\\c";

    ASSERT_TRUE(Utils::Files::FileSystem::create_directories(deep_directory_path,
                                                             &error) ) << error;
    EXPECT_TRUE(Utils::Files::FileSystem::is_directory(deep_directory_path) );

    /* Again: it is there already. */
    EXPECT_TRUE(Utils::Files::FileSystem::create_directories(deep_directory_path,
                                                             &error) ) << error;

    {
        std::ofstream output_file(deep_directory_path + "\\file.txt");

        output_file << "x";
    }

    EXPECT_TRUE (Utils::Files::FileSystem::is_file(deep_directory_path + "\\file.txt") );
    EXPECT_FALSE(Utils::Files::FileSystem::is_file(deep_directory_path) );
    EXPECT_FALSE(Utils::Files::FileSystem::is_directory_empty(deep_directory_path) );

    EXPECT_TRUE (Utils::Files::FileSystem::remove_tree(temp_directory.get_path() + "\\a") );
    EXPECT_FALSE(Utils::Files::FileSystem::is_directory(temp_directory.get_path() + "\\a") );

    /* What is not there is removed, as far as the caller is concerned. */
    EXPECT_TRUE (Utils::Files::FileSystem::remove_tree(temp_directory.get_path() + "\\a") );
}

TEST(FileSystemTest, AnEmptyDirectoryIsEmpty)
{
    const Tests::TempDirectory temp_directory;

    EXPECT_TRUE(Utils::Files::FileSystem::is_directory_empty(temp_directory.get_path() ) );
}

TEST(FileSystemTest, NamesWhichAreNotInAsciiAreKept)
{
    std::string                error;
    const Tests::TempDirectory temp_directory;

    const std::string non_ascii_directory_path = temp_directory.get_path() + "\\d\xC5\x82ugi katalog";

    ASSERT_TRUE(Utils::Files::FileSystem::create_directories(non_ascii_directory_path,
                                                             &error) ) << error;
    EXPECT_TRUE(Utils::Files::FileSystem::is_directory(non_ascii_directory_path) );
    EXPECT_EQ  (Utils::Files::FileSystem::to_utf8(Utils::Files::FileSystem::to_wide(non_ascii_directory_path) ),
                non_ascii_directory_path);
}

TEST(FileSystemTest, ADirectoryWhichCanBeWrittenToPassesTheTestAndLeavesNothing)
{
    std::string                error;
    const Tests::TempDirectory temp_directory;

    EXPECT_TRUE(Utils::Files::FileSystem::test_write(temp_directory.get_path(),
                                                     &error) ) << error;
    EXPECT_TRUE(Utils::Files::FileSystem::is_directory_empty(temp_directory.get_path() ) );
}

TEST(FileSystemTest, ADirectoryWhichCannotBeWrittenToFailsTheTestAndSaysWhere)
{
    std::string                error;
    const Tests::TempDirectory temp_directory;

    {
        std::ofstream output_file(temp_directory.get_path() + "\\blocker");

        output_file << "x";
    }

    /* Under a file nothing can be made. */
    EXPECT_FALSE(Utils::Files::FileSystem::test_write(temp_directory.get_path() + "\\blocker",
                                                      &error) );
    EXPECT_NE   (error.find("blocker"),
                 std::string::npos);

    EXPECT_FALSE(Utils::Files::FileSystem::create_directories(temp_directory.get_path() + "\\blocker\\sub",
                                                              &error) );
}
