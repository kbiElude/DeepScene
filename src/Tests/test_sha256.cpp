/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Tests for Sha256: the hash of bytes and of a file, with the values which the standard gives.
 */
#include <gtest/gtest.h>
#include "VR/AppInstaller/sha256.h"
#include "temp_directory.h"
#include "zip_builder.h"
#include <fstream>

TEST(Sha256Test, TheHashOfNothingAndOfAbcAreTheOnesOfTheStandard)
{
    std::string hex;

    ASSERT_TRUE(VR::AppInstaller::Sha256::hash_bytes("",
                                                     0,
                                                    &hex) );
    EXPECT_EQ   (hex,
                 "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");

    ASSERT_TRUE(VR::AppInstaller::Sha256::hash_bytes("abc",
                                                     3,
                                                    &hex) );
    EXPECT_EQ   (hex,
                 "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}

TEST(Sha256Test, BytesGivenInPiecesHaveTheHashOfTheWhole)
{
    VR::AppInstaller::Sha256 hasher;
    std::string              hex;

    ASSERT_TRUE(hasher.update("a",
                              1) );
    ASSERT_TRUE(hasher.update("bc",
                              2) );
    ASSERT_TRUE(hasher.finish(&hex) );

    EXPECT_EQ(hex,
              "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}

TEST(Sha256Test, TheHashOfAFileIsTheHashOfItsBytes)
{
    const Tests::TempDirectory directory;
    std::atomic<bool>          is_cancelled(false);
    std::string                error;
    std::string                hex;

    {
        std::ofstream file(directory.get_path() + "\\abc.bin",
                           std::ios::binary);

        file << "abc";
    }

    ASSERT_TRUE(VR::AppInstaller::Sha256::hash_file(directory.get_path() + "\\abc.bin",
                                                    is_cancelled,
                                                   &hex,
                                                   &error) ) << error;
    EXPECT_EQ   (hex,
                 "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}

TEST(Sha256Test, AFileWhichIsNotThereAndAHashWhichIsCancelledSaySo)
{
    const Tests::TempDirectory directory;
    std::atomic<bool>          is_cancelled(false);
    std::string                error;
    std::string                hex;

    EXPECT_FALSE(VR::AppInstaller::Sha256::hash_file(directory.get_path() + "\\nothing.bin",
                                                     is_cancelled,
                                                    &hex,
                                                    &error) );
    EXPECT_NE   (error.find("Cannot open"),
                 std::string::npos);

    {
        std::ofstream file(directory.get_path() + "\\some.bin",
                           std::ios::binary);

        file << "some";
    }

    is_cancelled = true;

    EXPECT_FALSE(VR::AppInstaller::Sha256::hash_file(directory.get_path() + "\\some.bin",
                                                     is_cancelled,
                                                    &hex,
                                                    &error) );
    EXPECT_EQ   (error,
                 "Cancelled");
}
