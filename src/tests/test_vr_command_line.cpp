/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Tests for the command line of the VR player.
 */
#include <gtest/gtest.h>
#include "vr_player/command_line.h"

TEST(VrCommandLineTest, NoArgumentsMeanTheCatalogNextToTheProgram)
{
    std::string       error;
    VRPlayer::Options options;

    ASSERT_TRUE(VRPlayer::CommandLine::parse({},
                                             &options,
                                             &error) ) << error;

    EXPECT_TRUE(options.catalog_filename.empty() );
}

TEST(VrCommandLineTest, TheCatalogCanBeNamed)
{
    std::string       error;
    VRPlayer::Options options;

    ASSERT_TRUE(VRPlayer::CommandLine::parse({"--catalog", "my apps.json"},
                                             &options,
                                             &error) ) << error;

    EXPECT_EQ(options.catalog_filename,
              "my apps.json");
}

TEST(VrCommandLineTest, ACatalogWithoutAFileAndUnknownArgumentsAreRefused)
{
    std::string       error;
    VRPlayer::Options options;

    EXPECT_FALSE(VRPlayer::CommandLine::parse({"--catalog"},
                                              &options,
                                              &error) );
    EXPECT_FALSE(error.empty() );

    EXPECT_FALSE(VRPlayer::CommandLine::parse({"--fullscreen"},
                                              &options,
                                              &error) );
    EXPECT_NE   (error.find("--fullscreen"),
                 std::string::npos);
}
