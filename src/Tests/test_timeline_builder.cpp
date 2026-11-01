/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Tests for TimelineBuilder: runs of frames of one shape and their speed, how often shapes and passes occur, and setup frames.
 */
#include <gtest/gtest.h>
#include "Analysis/APIDumpAnalysis/timeline_builder.h"

class TimelineBuilderTest : public ::testing::Test
{
protected:
    /* Adds a frame of the given duration right after the previous one; 1000 ticks per second. */
    void add_frame(const uint32_t& in_shape_id,
                   const uint64_t& in_n_draws,
                   const uint64_t& in_duration)
    {
        Analysis::APIDumpAnalysis::FrameRecord record;

        record.begin_timestamp_qpc = m_next_timestamp_qpc;
        record.end_timestamp_qpc   = m_next_timestamp_qpc + in_duration;
        record.frame_index         = m_analysis.model.frame_record_vec.size();
        record.n_draws             = in_n_draws;
        record.shape_id            = in_shape_id;

        m_next_timestamp_qpc += in_duration;

        m_analysis.model.frame_record_vec.push_back(record);
    }

    /* Returns the pass structure ID. */
    uint32_t add_pass(const uint64_t& in_key)
    {
        Analysis::APIDumpAnalysis::PassStructure structure;

        structure.key = in_key;

        return m_analysis.model.pass_structure_interner.intern(structure,
                                                               in_key);
    }

    uint32_t add_shape(const std::vector<uint32_t>& in_pass_id_vec)
    {
        Analysis::APIDumpAnalysis::FrameShape shape;

        shape.pass_structure_id_vec = in_pass_id_vec;

        return m_analysis.model.frame_shape_interner.intern(shape,
                                                            in_pass_id_vec.size() );
    }

    void SetUp() override
    {
        m_analysis.summary.first_timestamp_qpc = 0;
        m_analysis.summary.qpc_frequency       = 1000;
        m_next_timestamp_qpc                   = 0;
    }

    Analysis::APIDumpAnalysis::AnalysisResult m_analysis;
    uint64_t                  m_next_timestamp_qpc = 0;
};

TEST_F(TimelineBuilderTest, AnAnalysisWithNothingInItHasAnEmptyTimeline)
{
    const auto timeline = Analysis::APIDumpAnalysis::TimelineBuilder::build(m_analysis);

    EXPECT_TRUE(timeline.pass_usage_vec.empty() );
    EXPECT_TRUE(timeline.run_vec.empty() );
    EXPECT_TRUE(timeline.setup_frame_index_vec.empty() );
    EXPECT_TRUE(timeline.shape_usage_vec.empty() );
}

TEST_F(TimelineBuilderTest, FramesOfOneShapeInARowAreOneRun)
{
    const auto pass_id  = add_pass(1);
    const auto shape_a  = add_shape({pass_id});
    const auto shape_b  = add_shape({pass_id, pass_id});

    add_frame(shape_a,
              10,
              10);
    add_frame(shape_a,
              12,
              10);
    add_frame(shape_b,
              20,
              20);
    add_frame(shape_a,
              5,
              10);

    const auto timeline = Analysis::APIDumpAnalysis::TimelineBuilder::build(m_analysis);

    ASSERT_EQ(timeline.run_vec.size(),
              3u);

    EXPECT_EQ(timeline.run_vec.at(0).shape_id,
              shape_a);
    EXPECT_EQ(timeline.run_vec.at(0).n_frames,
              2u);
    EXPECT_EQ(timeline.run_vec.at(0).n_draws,
              22u);
    EXPECT_EQ(timeline.run_vec.at(0).first_frame_index,
              0u);
    EXPECT_DOUBLE_EQ(timeline.run_vec.at(0).begin_seconds,
                     0.0);
    EXPECT_DOUBLE_EQ(timeline.run_vec.at(0).end_seconds,
                     0.02);
    EXPECT_DOUBLE_EQ(timeline.run_vec.at(0).fps,
                     100.0);

    EXPECT_EQ(timeline.run_vec.at(1).shape_id,
              shape_b);
    EXPECT_EQ(timeline.run_vec.at(1).first_frame_index,
              2u);
    EXPECT_DOUBLE_EQ(timeline.run_vec.at(1).fps,
                     50.0);

    EXPECT_EQ(timeline.run_vec.at(2).shape_id,
              shape_a);
    EXPECT_EQ(timeline.run_vec.at(2).first_frame_index,
              3u);
}

TEST_F(TimelineBuilderTest, ARunThatTookNoTimeHasNoSpeed)
{
    const auto shape_id = add_shape({add_pass(1)});

    add_frame(shape_id,
              1,
              0);

    const auto timeline = Analysis::APIDumpAnalysis::TimelineBuilder::build(m_analysis);

    ASSERT_EQ(timeline.run_vec.size(),
              1u);

    EXPECT_DOUBLE_EQ(timeline.run_vec.at(0).fps,
                     0.0);
}

TEST_F(TimelineBuilderTest, ShapesSayWhenAndHowOftenTheyOccurred)
{
    const auto pass_id = add_pass(1);
    const auto shape_a = add_shape({pass_id});
    const auto shape_b = add_shape({pass_id, pass_id});

    add_frame(shape_a,
              1,
              10);
    add_frame(shape_b,
              1,
              10);
    add_frame(shape_a,
              1,
              10);

    const auto timeline = Analysis::APIDumpAnalysis::TimelineBuilder::build(m_analysis);

    ASSERT_EQ(timeline.shape_usage_vec.size(),
              2u);

    EXPECT_EQ(timeline.shape_usage_vec.at(shape_a).n_frames,
              2u);
    EXPECT_EQ(timeline.shape_usage_vec.at(shape_a).n_runs,
              2u);
    EXPECT_DOUBLE_EQ(timeline.shape_usage_vec.at(shape_a).first_seconds,
                     0.0);
    EXPECT_DOUBLE_EQ(timeline.shape_usage_vec.at(shape_a).last_seconds,
                     0.03);
    EXPECT_EQ(timeline.shape_usage_vec.at(shape_b).n_frames,
              1u);
    EXPECT_EQ(timeline.shape_usage_vec.at(shape_b).n_runs,
              1u);
    EXPECT_EQ(timeline.shape_usage_vec.at(shape_b).shape_id,
              shape_b);
}

TEST_F(TimelineBuilderTest, PassesSayInHowManyFramesAndInstancesTheyOccurred)
{
    const auto pass_0  = add_pass(1);
    const auto pass_1  = add_pass(2);
    const auto shape_a = add_shape({pass_0, pass_0, pass_1});
    const auto shape_b = add_shape({pass_1});

    add_frame(shape_a,
              1,
              10);
    add_frame(shape_a,
              1,
              10);
    add_frame(shape_b,
              1,
              10);

    const auto timeline = Analysis::APIDumpAnalysis::TimelineBuilder::build(m_analysis);

    ASSERT_EQ(timeline.pass_usage_vec.size(),
              2u);

    EXPECT_EQ(timeline.pass_usage_vec.at(pass_0).n_frames,
              2u);
    EXPECT_EQ(timeline.pass_usage_vec.at(pass_0).n_instances,
              4u);
    EXPECT_EQ(timeline.pass_usage_vec.at(pass_1).n_frames,
              3u);
    EXPECT_EQ(timeline.pass_usage_vec.at(pass_1).n_instances,
              3u);
    EXPECT_EQ(timeline.pass_usage_vec.at(pass_1).pass_structure_id,
              pass_1);
}

TEST_F(TimelineBuilderTest, AFrameThatTookFarLongerThanTheUsualOneIsSetup)
{
    const auto shape_id = add_shape({add_pass(1)});

    for (uint32_t n_frame = 0;
                  n_frame < 9;
                ++n_frame)
    {
        add_frame(shape_id,
                  1,
                  10);
    }

    add_frame(shape_id,
              1,
              5000);

    const auto timeline = Analysis::APIDumpAnalysis::TimelineBuilder::build(m_analysis);

    ASSERT_EQ(timeline.setup_frame_index_vec.size(),
              1u);
    EXPECT_EQ(timeline.setup_frame_index_vec.at(0),
              9u);
}

TEST_F(TimelineBuilderTest, FramesOfAboutTheSameDurationAreNotSetup)
{
    const auto shape_id = add_shape({add_pass(1)});

    add_frame(shape_id,
              1,
              10);
    add_frame(shape_id,
              1,
              30);
    add_frame(shape_id,
              1,
              10);

    EXPECT_TRUE(Analysis::APIDumpAnalysis::TimelineBuilder::build(m_analysis).setup_frame_index_vec.empty() );
}
