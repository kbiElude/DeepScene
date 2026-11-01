/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Tests for the command line of the stereo host.
 */
#include <gtest/gtest.h>
#include "Stereo/StereoPresenter/command_line.h"

TEST(StereoPresenterCommandLineTest, TheSessionAndTheTitleAreRead)
{
    std::string                    error;
    Stereo::StereoPresenter::Options options;

    ASSERT_TRUE(Stereo::StereoPresenter::CommandLine::parse({"--session", "12-34", "--title", "fr08 in VR"},
                                                         &options,
                                                         &error) ) << error;

    EXPECT_EQ(options.session_id,
              "12-34");
    EXPECT_EQ(options.title,
              "fr08 in VR");
}

TEST(StereoPresenterCommandLineTest, OnlyTheSessionIsNeeded)
{
    std::string                    error;
    Stereo::StereoPresenter::Options options;

    ASSERT_TRUE(Stereo::StereoPresenter::CommandLine::parse({"--session", "s"},
                                                         &options,
                                                         &error) ) << error;

    EXPECT_TRUE(options.title.empty() );

    EXPECT_FALSE(Stereo::StereoPresenter::CommandLine::parse({},
                                                          &options,
                                                          &error) );
    EXPECT_NE   (error.find("--session"),
                 std::string::npos);
}

TEST(StereoPresenterCommandLineTest, WhatIsNotRightIsRefused)
{
    std::string                    error;
    Stereo::StereoPresenter::Options options;

    EXPECT_FALSE(Stereo::StereoPresenter::CommandLine::parse({"--session"},
                                                          &options,
                                                          &error) );
    EXPECT_NE   (error.find("needs a value"),
                 std::string::npos);

    EXPECT_FALSE(Stereo::StereoPresenter::CommandLine::parse({"--session", "s", "--fullscreen"},
                                                          &options,
                                                          &error) );
    EXPECT_NE   (error.find("--fullscreen"),
                 std::string::npos);
}

TEST(StereoPresenterCommandLineTest, TheHeadsetIsUsedOnlyIfAskedFor)
{
    std::string                    error;
    Stereo::StereoPresenter::Options options;

    ASSERT_TRUE(Stereo::StereoPresenter::CommandLine::parse({"--session", "s"},
                                                         &options,
                                                         &error) ) << error;

    EXPECT_FALSE(options.use_headset);

    ASSERT_TRUE(Stereo::StereoPresenter::CommandLine::parse({"--headset", "--session", "s"},
                                                         &options,
                                                         &error) ) << error;

    EXPECT_TRUE(options.use_headset);
}

TEST(StereoPresenterCommandLineTest, TheLogIsWrittenOnlyIfAskedFor)
{
    std::string                    error;
    Stereo::StereoPresenter::Options options;

    ASSERT_TRUE(Stereo::StereoPresenter::CommandLine::parse({"--session", "s"},
                                                         &options,
                                                         &error) ) << error;

    EXPECT_FALSE(options.use_log);

    ASSERT_TRUE(Stereo::StereoPresenter::CommandLine::parse({"--log", "--session", "s"},
                                                         &options,
                                                         &error) ) << error;

    EXPECT_TRUE(options.use_log);
}
