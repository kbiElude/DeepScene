/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Tests for the command line of the player: what the options mean, what is refused, and what is handed over to the 32-bit player.
 */
#include <gtest/gtest.h>
#include "command_line.h"

static bool parse(std::vector<const char*>         in_argument_vec,
                  Stereo::Replay::Player::Options* out_options_ptr,
                  std::string*                     out_error_ptr)
{
    in_argument_vec.insert(in_argument_vec.begin(),
                           "StereoReplayPlayer");

    return Stereo::Replay::Player::CommandLine::parse(static_cast<int>(in_argument_vec.size() ),
                                                      const_cast<char**>(in_argument_vec.data() ),
                                                      out_options_ptr,
                                                      out_error_ptr);
}

TEST(CommandLineTest, AWorkloadAloneIsPlayedAsItIs)
{
    Stereo::Replay::Player::Options options;
    std::string                     error;

    ASSERT_TRUE(parse({"a.workload"},
                      &options,
                      &error) );

    EXPECT_EQ(options.workload_path,
              "a.workload");
    EXPECT_FALSE(options.has_eye_mode);
    EXPECT_FALSE(options.has_frame_range);
    EXPECT_FALSE(options.is_console);
    EXPECT_TRUE(options.recipe_filename.empty() );
}

TEST(CommandLineTest, TheRecipeAndTheEyeAreRead)
{
    Stereo::Replay::Player::Options options;
    std::string                     error;

    ASSERT_TRUE(parse({"--recipe", "r.json", "--eye", "right", "a.workload"},
                      &options,
                      &error) );

    EXPECT_EQ(options.recipe_filename,
              "r.json");
    EXPECT_TRUE(options.has_eye_mode);
    EXPECT_EQ(options.eye_mode,
              Stereo::Replay::Player::EyeMode::RIGHT);
}

TEST(CommandLineTest, AnEyeWithoutARecipeIsRefused)
{
    Stereo::Replay::Player::Options options;
    std::string                     error;

    EXPECT_FALSE(parse({"--eye", "left"},
                       &options,
                       &error) );
    EXPECT_FALSE(error.empty() );
}

TEST(CommandLineTest, TheEyeSeparationIsReadAndNeedsARecipeAndANumber)
{
    Stereo::Replay::Player::Options options;
    std::string                     error;

    ASSERT_TRUE(parse({"--recipe", "r.json", "--eye-separation", "0.25"},
                      &options,
                      &error) );

    EXPECT_TRUE(options.has_eye_separation);
    EXPECT_FLOAT_EQ(options.eye_separation,
                    0.25f);

    EXPECT_FALSE(parse({"--eye-separation", "0.25"},
                       &options,
                       &error) );
    EXPECT_FALSE(parse({"--recipe", "r.json", "--eye-separation", "wide"},
                       &options,
                       &error) );
    EXPECT_FALSE(parse({"--recipe", "r.json", "--eye-separation", "-1"},
                       &options,
                       &error) );
    EXPECT_FALSE(parse({"--recipe", "r.json", "--eye-separation"},
                       &options,
                       &error) );
}

TEST(CommandLineTest, TheFrameRangeIsRead)
{
    Stereo::Replay::Player::Options options;
    std::string                     error;

    ASSERT_TRUE(parse({"--frames", "20-35"},
                      &options,
                      &error) );

    EXPECT_TRUE(options.has_frame_range);
    EXPECT_EQ(options.first_frame,
              20u);
    EXPECT_EQ(options.last_frame,
              35u);
}

TEST(CommandLineTest, AFrameRangeOfOneFrameIsAllowed)
{
    Stereo::Replay::Player::Options options;
    std::string                     error;

    ASSERT_TRUE(parse({"--frames", "7-7"},
                      &options,
                      &error) );
}

TEST(CommandLineTest, AFrameRangeWhichIsNotOneIsRefused)
{
    const std::vector<const char*> range_vec = {"", "5", "5-", "-5", "a-b", "9-3", "1-2x", "1-2-3", "-5-3", "0-4294967296", "1e2-3", " 1-2", "+1-2"};

    for (const auto& range : range_vec)
    {
        Stereo::Replay::Player::Options options;
        std::string                     error;

        EXPECT_FALSE(parse({"--frames", range},
                           &options,
                           &error) ) << range;
    }

    {
        Stereo::Replay::Player::Options options;
        std::string                     error;

        EXPECT_FALSE(parse({"--frames"},
                           &options,
                           &error) );
    }
}

TEST(CommandLineTest, UnknownOptionsAndSecondWorkloadsAreRefused)
{
    Stereo::Replay::Player::Options options;
    std::string                     error;

    EXPECT_FALSE(parse({"--nope"},
                       &options,
                       &error) );
    EXPECT_FALSE(parse({"a.workload", "b.workload"},
                       &options,
                       &error) );
}

TEST(CommandLineTest, EverythingButTheWorkloadIsHandedOver)
{
    Stereo::Replay::Player::Options options;
    std::string                     error;

    ASSERT_TRUE(parse({"--console", "--recipe", "r.json", "--eye", "both", "--eye-separation", "0.25", "--frames", "2-9", "a.workload"},
                      &options,
                      &error) );

    const std::vector<std::string> expected_vec = {"--console", "--recipe", "r.json", "--eye", "both", "--eye-separation", "0.250000", "--frames", "2-9"};

    EXPECT_EQ(Stereo::Replay::Player::CommandLine::get_handover_argument_vec(options),
              expected_vec);
}
