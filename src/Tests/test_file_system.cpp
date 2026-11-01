/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Tests for FileSystem: directories which are made and removed, and the test of whether a directory can be written to.
 */
#include <gtest/gtest.h>
#include "VR/AppInstaller/file_system.h"
#include "temp_directory.h"
#include <fstream>

TEST(FileSystemTest, ADirectoryIsMadeWithTheOnesAboveItAndRemovedWithWhatIsInIt)
{
    const Tests::TempDirectory directory;
    std::string                error;
    const std::string          deep = directory.get_path() + "\\a\\b\\c";

    ASSERT_TRUE(VR::AppInstaller::FileSystem::create_directories(deep,
                                                                &error) ) << error;
    EXPECT_TRUE(VR::AppInstaller::FileSystem::is_directory(deep) );

    /* Again: it is there already. */
    EXPECT_TRUE(VR::AppInstaller::FileSystem::create_directories(deep,
                                                                &error) ) << error;

    {
        std::ofstream file(deep + "\\file.txt");

        file << "x";
    }

    EXPECT_TRUE (VR::AppInstaller::FileSystem::is_file(deep + "\\file.txt") );
    EXPECT_FALSE(VR::AppInstaller::FileSystem::is_file(deep) );
    EXPECT_FALSE(VR::AppInstaller::FileSystem::is_directory_empty(deep) );

    EXPECT_TRUE (VR::AppInstaller::FileSystem::remove_tree(directory.get_path() + "\\a") );
    EXPECT_FALSE(VR::AppInstaller::FileSystem::is_directory(directory.get_path() + "\\a") );

    /* What is not there is removed, as far as the caller is concerned. */
    EXPECT_TRUE (VR::AppInstaller::FileSystem::remove_tree(directory.get_path() + "\\a") );
}

TEST(FileSystemTest, AnEmptyDirectoryIsEmpty)
{
    const Tests::TempDirectory directory;

    EXPECT_TRUE(VR::AppInstaller::FileSystem::is_directory_empty(directory.get_path() ) );
}

TEST(FileSystemTest, NamesWhichAreNotInAsciiAreKept)
{
    const Tests::TempDirectory directory;
    std::string                error;
    const std::string          name = directory.get_path() + "\\d\xC5\x82ugi katalog";

    ASSERT_TRUE(VR::AppInstaller::FileSystem::create_directories(name,
                                                                &error) ) << error;
    EXPECT_TRUE(VR::AppInstaller::FileSystem::is_directory(name) );
    EXPECT_EQ  (VR::AppInstaller::FileSystem::to_utf8(VR::AppInstaller::FileSystem::to_wide(name) ),
                name);
}

TEST(FileSystemTest, ADirectoryWhichCanBeWrittenToPassesTheTestAndLeavesNothing)
{
    const Tests::TempDirectory directory;
    std::string                error;

    EXPECT_TRUE(VR::AppInstaller::FileSystem::test_write(directory.get_path(),
                                                        &error) ) << error;
    EXPECT_TRUE(VR::AppInstaller::FileSystem::is_directory_empty(directory.get_path() ) );
}

TEST(FileSystemTest, ADirectoryWhichCannotBeWrittenToFailsTheTestAndSaysWhere)
{
    const Tests::TempDirectory directory;
    std::string                error;

    {
        std::ofstream file(directory.get_path() + "\\blocker");

        file << "x";
    }

    /* Under a file nothing can be made. */
    EXPECT_FALSE(VR::AppInstaller::FileSystem::test_write(directory.get_path() + "\\blocker",
                                                         &error) );
    EXPECT_NE   (error.find("blocker"),
                 std::string::npos);

    EXPECT_FALSE(VR::AppInstaller::FileSystem::create_directories(directory.get_path() + "\\blocker\\sub",
                                                                 &error) );
}
