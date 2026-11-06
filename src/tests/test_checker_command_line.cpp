/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Tests for the command line of the replay check tool.
 */
#include <gtest/gtest.h>
#include "tools/stereo_replay/consistency_checker/command_line.h"

TEST(CheckerCommandLineTest, OnlyTheWorkloadIsNeeded)
{
    std::string                                 error;
    Stereo::Replay::ConsistencyChecker::Options options;

    ASSERT_TRUE(Stereo::Replay::ConsistencyChecker::CommandLine::parse({"a.workload"},
                                                                       &options,
                                                                       &error) ) << error;

    EXPECT_EQ(options.workload_filename,
              "a.workload");
    EXPECT_EQ(options.first_frame,
              0u);
    EXPECT_EQ(options.last_frame,
              999u);
    EXPECT_EQ(options.screenshot_frame,
              UINT32_MAX);
    EXPECT_EQ(options.contact_sheet_n_frames,
              0u);
    EXPECT_TRUE(options.way_name_vec.empty() );
    EXPECT_FALSE(options.is_parallel);
    EXPECT_FALSE(options.is_profiling);
}

TEST(CheckerCommandLineTest, TheOptionsOfTheCheckAreRead)
{
    std::string                                 error;
    Stereo::Replay::ConsistencyChecker::Options options;

    ASSERT_TRUE(Stereo::Replay::ConsistencyChecker::CommandLine::parse({"--from", "3", "--to", "7", "a.workload", "--dump-dir", "out", "--dump-frame", "5",
                                                                        "--deps", "a.deps", "--recipe", "r.json", "--eye-separation", "0.25", "--history",
                                                                        "--parallel"},
                                                                       &options,
                                                                       &error) ) << error;

    EXPECT_EQ(options.first_frame,
              3u);
    EXPECT_EQ(options.last_frame,
              7u);
    EXPECT_EQ(options.dump_directory,
              "out");
    EXPECT_EQ(options.dump_frame,
              5u);
    EXPECT_EQ(options.deps_filename,
              "a.deps");
    EXPECT_EQ(options.recipe_filename,
              "r.json");
    EXPECT_FLOAT_EQ(options.eye_separation,
                    0.25f);
    EXPECT_TRUE(options.has_history);
    EXPECT_TRUE(options.is_parallel);
}

TEST(CheckerCommandLineTest, TheWaysAreListedAndTheListsAreAdded)
{
    std::string                                 error;
    Stereo::Replay::ConsistencyChecker::Options options;

    ASSERT_TRUE(Stereo::Replay::ConsistencyChecker::CommandLine::parse({"a.workload", "--ways", "again,twice", "--ways", "restore"},
                                                                       &options,
                                                                       &error) ) << error;

    ASSERT_EQ(options.way_name_vec.size(),
              3u);
    EXPECT_EQ(options.way_name_vec.at(0),
              "again");
    EXPECT_EQ(options.way_name_vec.at(1),
              "twice");
    EXPECT_EQ(options.way_name_vec.at(2),
              "restore");
}

TEST(CheckerCommandLineTest, TheFirstFrameIsNotAfterTheLast)
{
    std::string                                 error;
    Stereo::Replay::ConsistencyChecker::Options options;

    EXPECT_FALSE(Stereo::Replay::ConsistencyChecker::CommandLine::parse({"a.workload", "--from", "8", "--to", "7"},
                                                                        &options,
                                                                        &error) );
    EXPECT_NE   (error.find("--from"),
                 std::string::npos) << error;

    EXPECT_TRUE(Stereo::Replay::ConsistencyChecker::CommandLine::parse({"a.workload", "--from", "7", "--to", "7"},
                                                                       &options,
                                                                       &error) ) << error;
}

TEST(CheckerCommandLineTest, NumbersAreWholeAndInTheirRange)
{
    const std::vector<std::vector<std::string> > refused_vec =
    {
        {"a.workload", "--from", "abc"},
        {"a.workload", "--from", "-1"},
        {"a.workload", "--from", "4294967296"},
        {"a.workload", "--to", "1.5"},
        {"a.workload", "--jobs", "0"},
        {"a.workload", "--jobs", "64"},
        {"a.workload", "--out", "x.png", "--screenshot", "4294967295"},
        {"a.workload", "--eye-separation", "wide"}
    };

    for (const auto& argument_vec : refused_vec)
    {
        std::string                                 error;
        Stereo::Replay::ConsistencyChecker::Options options;

        EXPECT_FALSE(Stereo::Replay::ConsistencyChecker::CommandLine::parse(argument_vec,
                                                                            &options,
                                                                            &error) ) << argument_vec.back();
        EXPECT_FALSE(error.empty() ) << argument_vec.back();
    }

    {
        std::string                                 error;
        Stereo::Replay::ConsistencyChecker::Options options;

        EXPECT_TRUE(Stereo::Replay::ConsistencyChecker::CommandLine::parse({"a.workload", "--jobs", "63"},
                                                                           &options,
                                                                           &error) ) << error;
        EXPECT_EQ(options.n_jobs,
                  63u);
    }
}

TEST(CheckerCommandLineTest, AnOptionWithoutItsValueIsRefused)
{
    for (const char* option_ptr : {"--from", "--to", "--recipe", "--deps", "--dump-dir", "--dump-frame", "--eye", "--eye-separation", "--ways", "--out", "--screenshot",
                                   "--contact-sheet", "--jobs", "--same-frames-as", "--overlay-probe", "--overlay-probe-result"})
    {
        std::string                                 error;
        Stereo::Replay::ConsistencyChecker::Options options;

        EXPECT_FALSE(Stereo::Replay::ConsistencyChecker::CommandLine::parse({"a.workload", option_ptr},
                                                                            &options,
                                                                            &error) ) << option_ptr;
        EXPECT_NE   (error.find(option_ptr),
                     std::string::npos) << error;
    }
}

TEST(CheckerCommandLineTest, AWorkloadIsNeededAndOnlyOneIsTaken)
{
    std::string                                 error;
    Stereo::Replay::ConsistencyChecker::Options options;

    EXPECT_FALSE(Stereo::Replay::ConsistencyChecker::CommandLine::parse({},
                                                                        &options,
                                                                        &error) );
    EXPECT_FALSE(Stereo::Replay::ConsistencyChecker::CommandLine::parse({"--profile"},
                                                                        &options,
                                                                        &error) );
    EXPECT_FALSE(Stereo::Replay::ConsistencyChecker::CommandLine::parse({"a.workload", "b.workload"},
                                                                        &options,
                                                                        &error) );
    EXPECT_FALSE(Stereo::Replay::ConsistencyChecker::CommandLine::parse({"a.workload", "--no-such-option"},
                                                                        &options,
                                                                        &error) );
    EXPECT_NE   (error.find("--no-such-option"),
                 std::string::npos) << error;
}

TEST(CheckerCommandLineTest, TheImageOptionsGoTogether)
{
    std::string                                 error;
    Stereo::Replay::ConsistencyChecker::Options options;

    /* A screenshot or a sheet needs the file, and the file needs one of them. */
    EXPECT_TRUE (Stereo::Replay::ConsistencyChecker::CommandLine::parse({"a.workload", "--screenshot", "5", "--out", "x.png"},
                                                                        &options,
                                                                        &error) ) << error;
    EXPECT_EQ   (options.screenshot_frame,
                 5u);
    EXPECT_EQ   (options.image_filename,
                 "x.png");
    EXPECT_TRUE (Stereo::Replay::ConsistencyChecker::CommandLine::parse({"a.workload", "--contact-sheet", "4", "--out", "x.png"},
                                                                        &options,
                                                                        &error) ) << error;
    EXPECT_EQ   (options.contact_sheet_n_frames,
                 4u);
    EXPECT_FALSE(Stereo::Replay::ConsistencyChecker::CommandLine::parse({"a.workload", "--screenshot", "5"},
                                                                        &options,
                                                                        &error) );
    EXPECT_FALSE(Stereo::Replay::ConsistencyChecker::CommandLine::parse({"a.workload", "--out", "x.png"},
                                                                        &options,
                                                                        &error) );

    /* Not both of them. */
    EXPECT_FALSE(Stereo::Replay::ConsistencyChecker::CommandLine::parse({"a.workload", "--screenshot", "5", "--contact-sheet", "4", "--out", "x.png"},
                                                                        &options,
                                                                        &error) );
}

TEST(CheckerCommandLineTest, TheViewOfTheEyesIsOneOfFourAndNeedsARecipe)
{
    const std::vector<std::pair<std::string, Utils::Images::EyeView> > view_vec =
    {
        {"anaglyph", Utils::Images::EyeView::ANAGLYPH},
        {"both",     Utils::Images::EyeView::BOTH},
        {"left",     Utils::Images::EyeView::LEFT},
        {"right",    Utils::Images::EyeView::RIGHT}
    };

    for (const auto& view : view_vec)
    {
        std::string                                 error;
        Stereo::Replay::ConsistencyChecker::Options options;

        ASSERT_TRUE(Stereo::Replay::ConsistencyChecker::CommandLine::parse({"a.workload", "--screenshot", "5", "--out", "x.png", "--recipe", "r.json", "--eye", view.first},
                                                                           &options,
                                                                           &error) ) << view.first << ": " << error;
        EXPECT_EQ(options.eye_view,
                  view.second);
        EXPECT_TRUE(options.has_eye_view);
    }

    {
        std::string                                 error;
        Stereo::Replay::ConsistencyChecker::Options options;

        EXPECT_FALSE(Stereo::Replay::ConsistencyChecker::CommandLine::parse({"a.workload", "--screenshot", "5", "--out", "x.png", "--recipe", "r.json", "--eye", "purple"},
                                                                            &options,
                                                                            &error) );
        EXPECT_FALSE(Stereo::Replay::ConsistencyChecker::CommandLine::parse({"a.workload", "--screenshot", "5", "--out", "x.png", "--eye", "left"},
                                                                            &options,
                                                                            &error) );
    }
}

TEST(CheckerCommandLineTest, APartOfTheOverlayProbeHasItsNumberBelowTheNumberOfTheParts)
{
    std::string                                 error;
    Stereo::Replay::ConsistencyChecker::Options options;

    ASSERT_TRUE(Stereo::Replay::ConsistencyChecker::CommandLine::parse({"a.workload", "--overlay-probe-shard", "1", "3", "--overlay-probe-result", "part.json"},
                                                                       &options,
                                                                       &error) ) << error;
    EXPECT_EQ(options.overlay_probe_shard_index,
              1u);
    EXPECT_EQ(options.overlay_probe_shard_count,
              3u);
    EXPECT_EQ(options.overlay_probe_result_filename,
              "part.json");

    EXPECT_FALSE(Stereo::Replay::ConsistencyChecker::CommandLine::parse({"a.workload", "--overlay-probe-shard", "3", "3"},
                                                                        &options,
                                                                        &error) );
    EXPECT_FALSE(Stereo::Replay::ConsistencyChecker::CommandLine::parse({"a.workload", "--overlay-probe-shard", "1"},
                                                                        &options,
                                                                        &error) );
    EXPECT_FALSE(Stereo::Replay::ConsistencyChecker::CommandLine::parse({"a.workload", "--overlay-probe-shard", "x", "3"},
                                                                        &options,
                                                                        &error) );
}

TEST(CheckerCommandLineTest, TheUsageSaysWhatTheToolIs)
{
    const std::string usage = Stereo::Replay::ConsistencyChecker::CommandLine::get_usage_ptr();

    EXPECT_NE(usage.find("Usage:"),
              std::string::npos);
    EXPECT_NE(usage.find("--screenshot"),
              std::string::npos);
    EXPECT_NE(usage.find("--jobs"),
              std::string::npos);
}
