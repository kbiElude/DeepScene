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
    std::string hash_hex;

    ASSERT_TRUE(VR::AppInstaller::Sha256::hash_bytes("",
                                                     0,
                                                    &hash_hex) );
    EXPECT_EQ   (hash_hex,
                 "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");

    ASSERT_TRUE(VR::AppInstaller::Sha256::hash_bytes("abc",
                                                     3,
                                                    &hash_hex) );
    EXPECT_EQ   (hash_hex,
                 "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}

TEST(Sha256Test, BytesGivenInPiecesHaveTheHashOfTheWhole)
{
    std::string              hash_hex;
    VR::AppInstaller::Sha256 hasher;

    ASSERT_TRUE(hasher.update("a",
                              1) );
    ASSERT_TRUE(hasher.update("bc",
                              2) );
    ASSERT_TRUE(hasher.finish(&hash_hex) );

    EXPECT_EQ(hash_hex,
              "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}

TEST(Sha256Test, TheHashOfAFileIsTheHashOfItsBytes)
{
    std::string                error;
    std::string                hash_hex;
    std::atomic<bool>          is_cancelled(false);
    const Tests::TempDirectory temp_directory;

    {
        std::ofstream output_file(temp_directory.get_path() + "\\abc.bin",
                                  std::ios::binary);

        output_file << "abc";
    }

    ASSERT_TRUE(VR::AppInstaller::Sha256::hash_file(temp_directory.get_path() + "\\abc.bin",
                                                    is_cancelled,
                                                   &hash_hex,
                                                   &error) ) << error;
    EXPECT_EQ   (hash_hex,
                 "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}

TEST(Sha256Test, AFileWhichIsNotThereAndAHashWhichIsCancelledSaySo)
{
    std::string                error;
    std::string                hash_hex;
    std::atomic<bool>          is_cancelled(false);
    const Tests::TempDirectory temp_directory;

    EXPECT_FALSE(VR::AppInstaller::Sha256::hash_file(temp_directory.get_path() + "\\nothing.bin",
                                                     is_cancelled,
                                                    &hash_hex,
                                                    &error) );
    EXPECT_NE   (error.find("Cannot open"),
                 std::string::npos);

    {
        std::ofstream output_file(temp_directory.get_path() + "\\some.bin",
                                  std::ios::binary);

        output_file << "some";
    }

    is_cancelled = true;

    EXPECT_FALSE(VR::AppInstaller::Sha256::hash_file(temp_directory.get_path() + "\\some.bin",
                                                     is_cancelled,
                                                    &hash_hex,
                                                    &error) );
    EXPECT_EQ   (error,
                 "Cancelled");
}
