/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Tests for the command line of the VR player.
 */
#include <gtest/gtest.h>
#include "VR/VRPlayer/command_line.h"

TEST(VrCommandLineTest, NoArgumentsMeanTheCatalogNextToTheProgram)
{
    std::string      error;
    VR::VRPlayer::Options options;

    ASSERT_TRUE(VR::VRPlayer::CommandLine::parse({},
                                                &options,
                                                &error) ) << error;

    EXPECT_TRUE(options.catalog_filename.empty() );
}

TEST(VrCommandLineTest, TheCatalogCanBeNamed)
{
    std::string      error;
    VR::VRPlayer::Options options;

    ASSERT_TRUE(VR::VRPlayer::CommandLine::parse({"--catalog", "my apps.json"},
                                                &options,
                                                &error) ) << error;

    EXPECT_EQ(options.catalog_filename,
              "my apps.json");
}

TEST(VrCommandLineTest, ACatalogWithoutAFileAndUnknownArgumentsAreRefused)
{
    std::string      error;
    VR::VRPlayer::Options options;

    EXPECT_FALSE(VR::VRPlayer::CommandLine::parse({"--catalog"},
                                                 &options,
                                                 &error) );
    EXPECT_FALSE(error.empty() );

    EXPECT_FALSE(VR::VRPlayer::CommandLine::parse({"--fullscreen"},
                                                 &options,
                                                 &error) );
    EXPECT_NE   (error.find("--fullscreen"),
                 std::string::npos);
}
