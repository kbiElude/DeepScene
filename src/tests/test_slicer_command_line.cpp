/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Checks of the command line of the slicer.
 */
#include <gtest/gtest.h>
#include "tools/analysis/api_dump_slicer/command_line.h"

TEST(SlicerCommandLine, AFrameAloneAndAFrameFromZeroAreTheSame)
{
    Analysis::APIDumpSlicer::Options options;
    std::string                      error;

    ASSERT_TRUE(Analysis::APIDumpSlicer::CommandLine::parse({"in.workload", "out.workload", "--frames", "300"},
                                                           &options,
                                                           &error) ) << error;
    EXPECT_EQ(options.slice_options.last_frame,
              300u);
    EXPECT_EQ(options.workload_filename,
              "in.workload");
    EXPECT_EQ(options.slice_filename,
              "out.workload");

    ASSERT_TRUE(Analysis::APIDumpSlicer::CommandLine::parse({"--frames", "0-300", "in.workload", "out.workload"},
                                                           &options,
                                                           &error) ) << error;
    EXPECT_EQ(options.slice_options.last_frame,
              300u);
}

TEST(SlicerCommandLine, ASliceWhichDoesNotStartWithTheFirstFrameIsRefused)
{
    Analysis::APIDumpSlicer::Options options;
    std::string                      error;

    EXPECT_FALSE(Analysis::APIDumpSlicer::CommandLine::parse({"in.workload", "out.workload", "--frames", "5-300"},
                                                            &options,
                                                            &error) );
    EXPECT_FALSE(error.empty() );
}

TEST(SlicerCommandLine, TheOptionalValuesAreRead)
{
    Analysis::APIDumpSlicer::Options options;
    std::string                      error;

    ASSERT_TRUE(Analysis::APIDumpSlicer::CommandLine::parse({"in.workload", "out.workload", "--frames", "1", "--strip-chunks-above", "256", "--threads", "4"},
                                                           &options,
                                                           &error) ) << error;
    EXPECT_EQ(options.slice_options.strip_chunks_above_n_bytes,
              256u);
    EXPECT_EQ(options.slice_options.n_reader_threads,
              4u);
}

TEST(SlicerCommandLine, WhatIsMissingOrWrongIsSaid)
{
    Analysis::APIDumpSlicer::Options options;
    std::string                      error;

    EXPECT_FALSE(Analysis::APIDumpSlicer::CommandLine::parse({"in.workload", "out.workload"},
                                                            &options,
                                                            &error) ); /* no frames */
    EXPECT_FALSE(Analysis::APIDumpSlicer::CommandLine::parse({"in.workload", "--frames", "3"},
                                                            &options,
                                                            &error) ); /* no slice file */
    EXPECT_FALSE(Analysis::APIDumpSlicer::CommandLine::parse({"in.workload", "in.workload", "--frames", "3"},
                                                            &options,
                                                            &error) ); /* over the workload */
    EXPECT_FALSE(Analysis::APIDumpSlicer::CommandLine::parse({"in.workload", "out.workload", "--frames", "x"},
                                                            &options,
                                                            &error) );
    EXPECT_FALSE(Analysis::APIDumpSlicer::CommandLine::parse({"in.workload", "out.workload", "--frames", "3", "--bogus"},
                                                            &options,
                                                            &error) );
    EXPECT_FALSE(Analysis::APIDumpSlicer::CommandLine::parse({"a", "b", "c", "--frames", "3"},
                                                            &options,
                                                            &error) );
}
