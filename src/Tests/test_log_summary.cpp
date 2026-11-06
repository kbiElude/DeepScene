/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Tests for the summary that the launcher gives of the log of the converter.
 */
#include <gtest/gtest.h>
#include "runtime/StereoLauncher/log_summary.h"

namespace
{
    const char* const STATISTICS_LINE_PTR = "12:00:01 After 600 frames: 1200 affected draws, 300 unaffected, 20 skipped, 5 of a signature the recipe does not know, "
                                            "7 frames of a scene it has no distance for, 40 matrices kept, 11 surfaces with a twin, 2 twins that could not be made, "
                                            "9 calls not made for the right eye, 0 calls on the back buffer in pixels it no longer has, the callbacks took 0.4 ms a frame";
}

TEST(LogSummary, TheLastLineOfStatisticsIsTheSummary)
{
    const auto summary = Stereo::Launcher::LogSummarizer::summarize({"the converter starts",
                                                                     "After 200 frames: 1 affected draws, 2 unaffected, 3 skipped, 4 of a signature the recipe does not know, 5 frames of a scene it has no distance for, 6 matrices kept",
                                                                     STATISTICS_LINE_PTR,
                                                                     "something else"});

    EXPECT_TRUE(summary.has_statistics);
    EXPECT_EQ(summary.n_frames,
              600u);
    EXPECT_EQ(summary.n_affected_draws,
              1200u);
    EXPECT_EQ(summary.n_unaffected_draws,
              300u);
    EXPECT_EQ(summary.n_skipped_draws,
              20u);
    EXPECT_EQ(summary.n_unknown_draws,
              5u);
    EXPECT_EQ(summary.n_unknown_frames,
              7u);
    EXPECT_EQ(summary.n_failed_twins,
              2u);
    EXPECT_EQ(summary.n_unmirrored_calls,
              9u);
}

TEST(LogSummary, ALogWithoutStatisticsSaysSo)
{
    const auto summary = Stereo::Launcher::LogSummarizer::summarize({"After the window is made, nothing", "other lines"});
    const auto line_vec = Stereo::Launcher::LogSummarizer::make_report_lines(summary);

    EXPECT_FALSE(summary.has_statistics);
    ASSERT_EQ(line_vec.size(),
              1u);
    EXPECT_NE(line_vec.at(0).find("no statistics"),
              std::string::npos);
}

TEST(LogSummary, EachThingTheConverterCouldNotDoIsALineOfItsOwn)
{
    const auto line_vec = Stereo::Launcher::LogSummarizer::make_report_lines(Stereo::Launcher::LogSummarizer::summarize({STATISTICS_LINE_PTR}) );

    /* The verdict, and the four things that went wrong. */
    ASSERT_EQ(line_vec.size(),
              5u);
    EXPECT_EQ(line_vec.at(0).substr(0,
                                    4),
              "[*] ");
    EXPECT_NE(line_vec.at(1).find("5 draws"),
              std::string::npos);
    EXPECT_NE(line_vec.at(2).find("7 frames"),
              std::string::npos);
    EXPECT_NE(line_vec.at(3).find("2 surfaces"),
              std::string::npos);
    EXPECT_NE(line_vec.at(4).find("9 calls"),
              std::string::npos);
}

TEST(LogSummary, ARunWithNothingWrongIsOneLine)
{
    const auto line_vec = Stereo::Launcher::LogSummarizer::make_report_lines(Stereo::Launcher::LogSummarizer::summarize({"After 600 frames: 10 affected draws, 2 unaffected, 0 skipped, 0 of a signature the recipe does not know, 0 frames of a scene it has no distance for, 3 matrices kept"}) );

    ASSERT_EQ(line_vec.size(),
              1u);
}
