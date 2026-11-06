/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Tests for the command line of the stereo launcher.
 */
#include <gtest/gtest.h>
#include "runtime/StereoLauncher/command_line.h"

TEST(StereoLauncherCommandLineTest, EverythingIsRead)
{
    std::string               error;
    Stereo::Launcher::Options options;

    ASSERT_TRUE(Stereo::Launcher::CommandLine::parse({"--recipe", "r.json", "--session", "7-9", "--arguments", "-a \"b c\"",
                                                      "--working-directory", "C:\\work", "--wait", "--log", "C:\\demo\\demo.exe"},
                                                     &options,
                                                     &error) ) << error;

    EXPECT_EQ(options.recipe_path,
              "r.json");
    EXPECT_EQ(options.session_id,
              "7-9");
    EXPECT_EQ(options.arguments,
              "-a \"b c\"");
    EXPECT_EQ(options.working_directory,
              "C:\\work");
    EXPECT_EQ(options.executable_path,
              "C:\\demo\\demo.exe");
    EXPECT_TRUE(options.should_wait);
    EXPECT_TRUE(options.is_logging);
}

TEST(StereoLauncherCommandLineTest, OnlyTheApplicationTheRecipeAndTheSessionAreNeeded)
{
    std::string               error;
    Stereo::Launcher::Options options;

    ASSERT_TRUE(Stereo::Launcher::CommandLine::parse({"--session", "s", "--recipe", "r.json", "demo.exe"},
                                                     &options,
                                                     &error) ) << error;

    EXPECT_FALSE(options.should_wait);
    EXPECT_FALSE(options.is_logging);
    EXPECT_TRUE (options.arguments.empty() );
    EXPECT_TRUE (options.working_directory.empty() );

    EXPECT_FALSE(Stereo::Launcher::CommandLine::parse({"--session", "s", "--recipe", "r.json"},
                                                      &options,
                                                      &error) );
    EXPECT_NE   (error.find("application"),
                 std::string::npos);

    EXPECT_FALSE(Stereo::Launcher::CommandLine::parse({"--session", "s", "demo.exe"},
                                                      &options,
                                                      &error) );
    EXPECT_NE   (error.find("--recipe"),
                 std::string::npos);

    EXPECT_FALSE(Stereo::Launcher::CommandLine::parse({"--recipe", "r.json", "demo.exe"},
                                                      &options,
                                                      &error) );
    EXPECT_NE   (error.find("--session"),
                 std::string::npos);
}

TEST(StereoLauncherCommandLineTest, WhatIsNotRightIsRefused)
{
    std::string               error;
    Stereo::Launcher::Options options;

    EXPECT_FALSE(Stereo::Launcher::CommandLine::parse({"--recipe"},
                                                      &options,
                                                      &error) );
    EXPECT_NE   (error.find("needs a value"),
                 std::string::npos);

    EXPECT_FALSE(Stereo::Launcher::CommandLine::parse({"--fullscreen", "demo.exe"},
                                                      &options,
                                                      &error) );
    EXPECT_NE   (error.find("--fullscreen"),
                 std::string::npos);

    EXPECT_FALSE(Stereo::Launcher::CommandLine::parse({"a.exe", "b.exe"},
                                                      &options,
                                                      &error) );
    EXPECT_NE   (error.find("More than one"),
                 std::string::npos);
}
