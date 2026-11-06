/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Tests for ResourceAccessTracker: which resources a frame can be played twice with, and which not.
 */
#include <gtest/gtest.h>
#include "analysis/api_dump_analysis/resource_access_tracker.h"

class ResourceAccessTrackerTest : public ::testing::Test
{
protected:
    static constexpr uint32_t N_RESOURCES = 4;

    static Analysis::APIDumpAnalysis::ResourceId get_id(const uint32_t& in_n_resource)
    {
        return Analysis::APIDumpAnalysis::make_resource_id(in_n_resource);
    }

    void end_frame()
    {
        m_tracker.end_frame(m_n_frame,
                           &m_access_vec,
                           &m_loop_carried_id_vec);

        ++m_n_frame;
    }

    Analysis::APIDumpAnalysis::ResourceAccessClass get_class(const uint32_t& in_n_resource) const
    {
        return Analysis::APIDumpAnalysis::ResourceAccessTracker::classify(m_access_vec.at(in_n_resource - 1) );
    }

    void SetUp() override
    {
        m_access_vec.resize(N_RESOURCES);
    }

    std::vector<Analysis::APIDumpAnalysis::ResourceAccess> m_access_vec;
    std::vector<Analysis::APIDumpAnalysis::ResourceId>     m_loop_carried_id_vec;
    uint64_t                                               m_n_frame = 0;
    Analysis::APIDumpAnalysis::ResourceAccessTracker       m_tracker;
};

TEST_F(ResourceAccessTrackerTest, AResourceNoFrameTouchedIsUnused)
{
    m_tracker.on_write(get_id(1) );

    end_frame();

    EXPECT_EQ(get_class(2),
              Analysis::APIDumpAnalysis::ResourceAccessClass::UNUSED);
}

TEST_F(ResourceAccessTrackerTest, AResourceWrittenBeforeItIsReadInEachFrameIsFrameLocal)
{
    for (int n_frame = 0;
             n_frame < 5;
           ++n_frame)
    {
        m_tracker.on_write(get_id(1) );
        m_tracker.on_read (get_id(1) );

        end_frame();
    }

    EXPECT_EQ(get_class(1),
              Analysis::APIDumpAnalysis::ResourceAccessClass::FRAME_LOCAL);
    EXPECT_EQ(m_access_vec.at(0).n_frames_read,
              5u);
    EXPECT_EQ(m_access_vec.at(0).n_frames_written,
              5u);
    EXPECT_EQ(m_access_vec.at(0).n_frames_loop_carried,
              0u);
}

TEST_F(ResourceAccessTrackerTest, AResourceReadBeforeItIsWrittenInAFrameIsLoopCarried)
{
    for (int n_frame = 0;
             n_frame < 3;
           ++n_frame)
    {
        m_tracker.on_read (get_id(1) );
        m_tracker.on_write(get_id(1) );

        end_frame();
    }

    EXPECT_EQ(get_class(1),
              Analysis::APIDumpAnalysis::ResourceAccessClass::LOOP_CARRIED);
    EXPECT_EQ(m_access_vec.at(0).n_frames_loop_carried,
              3u);
}

TEST_F(ResourceAccessTrackerTest, OneFrameThatReadsBeforeItWritesIsEnough)
{
    m_tracker.on_write(get_id(1) );
    m_tracker.on_read (get_id(1) );

    end_frame();

    m_tracker.on_read (get_id(1) );
    m_tracker.on_write(get_id(1) );

    end_frame();

    m_tracker.on_write(get_id(1) );
    m_tracker.on_read (get_id(1) );

    end_frame();

    EXPECT_EQ(get_class(1),
              Analysis::APIDumpAnalysis::ResourceAccessClass::LOOP_CARRIED);
    EXPECT_EQ(m_access_vec.at(0).n_frames_loop_carried,
              1u);
}

TEST_F(ResourceAccessTrackerTest, ReadingInOneFrameAndWritingInAnotherIsNotLoopCarried)
{
    m_tracker.on_read(get_id(1) );

    end_frame();

    m_tracker.on_write(get_id(1) );

    end_frame();

    EXPECT_EQ(m_access_vec.at(0).n_frames_loop_carried,
              0u);
    EXPECT_NE(get_class(1),
              Analysis::APIDumpAnalysis::ResourceAccessClass::LOOP_CARRIED);
}

TEST_F(ResourceAccessTrackerTest, ReadingAfterWritingTwiceInAFrameIsStillNotLoopCarried)
{
    m_tracker.on_write(get_id(1) );
    m_tracker.on_read (get_id(1) );
    m_tracker.on_write(get_id(1) );
    m_tracker.on_read (get_id(1) );

    end_frame();

    EXPECT_EQ(m_access_vec.at(0).n_frames_loop_carried,
              0u);
}

TEST_F(ResourceAccessTrackerTest, AResourceWrittenOnlyUpToTheFrameThatFirstReadsItIsStatic)
{
    m_tracker.on_write(get_id(1) );
    m_tracker.on_read (get_id(1) );

    end_frame();

    for (int n_frame = 0;
             n_frame < 4;
           ++n_frame)
    {
        m_tracker.on_read(get_id(1) );

        end_frame();
    }

    EXPECT_EQ(get_class(1),
              Analysis::APIDumpAnalysis::ResourceAccessClass::STATIC);
}

TEST_F(ResourceAccessTrackerTest, AResourceThatIsOnlyReadIsStatic)
{
    m_tracker.on_read(get_id(1) );

    end_frame();

    EXPECT_EQ(get_class(1),
              Analysis::APIDumpAnalysis::ResourceAccessClass::STATIC);
}

TEST_F(ResourceAccessTrackerTest, AResourceWrittenAfterTheFirstFrameThatReadsItIsFrameLocal)
{
    m_tracker.on_write(get_id(1) );
    m_tracker.on_read (get_id(1) );

    end_frame();

    m_tracker.on_write(get_id(1) );
    m_tracker.on_read (get_id(1) );

    end_frame();

    EXPECT_EQ(get_class(1),
              Analysis::APIDumpAnalysis::ResourceAccessClass::FRAME_LOCAL);
}

TEST_F(ResourceAccessTrackerTest, AResourceWrittenAndNeverReadIsUnread)
{
    m_tracker.on_write(get_id(1) );

    end_frame();

    EXPECT_EQ(get_class(1),
              Analysis::APIDumpAnalysis::ResourceAccessClass::UNREAD);
}

TEST_F(ResourceAccessTrackerTest, FramesAreCountedOnceHoweverOftenTheyTouchAResource)
{
    for (int n_touch = 0;
             n_touch < 10;
           ++n_touch)
    {
        m_tracker.on_write(get_id(1) );
        m_tracker.on_read (get_id(1) );
    }

    end_frame();

    EXPECT_EQ(m_access_vec.at(0).n_frames_read,
              1u);
    EXPECT_EQ(m_access_vec.at(0).n_frames_written,
              1u);
}

TEST_F(ResourceAccessTrackerTest, TheFramesOfTheFirstReadAndOfTheFirstAndLastWriteAreKept)
{
    end_frame();
    end_frame();

    m_tracker.on_read(get_id(1) );

    end_frame();

    m_tracker.on_write(get_id(1) );

    end_frame();

    end_frame();

    m_tracker.on_write(get_id(1) );

    end_frame();

    EXPECT_EQ(m_access_vec.at(0).first_read_frame_index,
              2u);
    EXPECT_EQ(m_access_vec.at(0).first_write_frame_index,
              3u);
    EXPECT_EQ(m_access_vec.at(0).last_write_frame_index,
              5u);
}

TEST_F(ResourceAccessTrackerTest, TheResourceNoneIsIgnored)
{
    m_tracker.on_read (Analysis::APIDumpAnalysis::ResourceId::NONE);
    m_tracker.on_write(Analysis::APIDumpAnalysis::ResourceId::NONE);

    end_frame();

    for (const auto& access : m_access_vec)
    {
        EXPECT_EQ(Analysis::APIDumpAnalysis::ResourceAccessTracker::classify(access),
                  Analysis::APIDumpAnalysis::ResourceAccessClass::UNUSED);
    }
}

TEST_F(ResourceAccessTrackerTest, AFrameStartsWithNothingDone)
{
    m_tracker.on_write(get_id(1) );

    end_frame();

    /* A read in a new frame does not follow the previous frame's write. */
    m_tracker.on_read (get_id(1) );
    m_tracker.on_write(get_id(1) );

    end_frame();

    EXPECT_EQ(get_class(1),
              Analysis::APIDumpAnalysis::ResourceAccessClass::LOOP_CARRIED);
}

TEST_F(ResourceAccessTrackerTest, ResourcesAreCountedByClass)
{
    m_tracker.on_write(get_id(1) );
    m_tracker.on_read (get_id(2) );
    m_tracker.on_write(get_id(3) );
    m_tracker.on_read (get_id(3) );

    end_frame();

    m_tracker.on_read (get_id(3) );
    m_tracker.on_write(get_id(3) );

    end_frame();

    const auto count_map = Analysis::APIDumpAnalysis::ResourceAccessTracker::count_by_class(m_access_vec);

    EXPECT_EQ(count_map.size(),
              5u);
    EXPECT_EQ(count_map.at(Analysis::APIDumpAnalysis::ResourceAccessClass::LOOP_CARRIED),
              1u);
    EXPECT_EQ(count_map.at(Analysis::APIDumpAnalysis::ResourceAccessClass::STATIC),
              1u);
    EXPECT_EQ(count_map.at(Analysis::APIDumpAnalysis::ResourceAccessClass::UNREAD),
              1u);
    EXPECT_EQ(count_map.at(Analysis::APIDumpAnalysis::ResourceAccessClass::UNUSED),
              1u);
    EXPECT_EQ(count_map.at(Analysis::APIDumpAnalysis::ResourceAccessClass::FRAME_LOCAL),
              0u);
}

TEST_F(ResourceAccessTrackerTest, TheResourcesOfAFrameThatAreLoopCarriedAreListedByID)
{
    m_tracker.on_read (get_id(3) );
    m_tracker.on_write(get_id(3) );
    m_tracker.on_write(get_id(2) );
    m_tracker.on_read (get_id(2) );
    m_tracker.on_read (get_id(1) );
    m_tracker.on_write(get_id(1) );
    m_tracker.on_read (get_id(4) );

    end_frame();

    const std::vector<Analysis::APIDumpAnalysis::ResourceId> expected_vec = {get_id(1), get_id(3)};

    EXPECT_EQ(m_loop_carried_id_vec,
              expected_vec);

    /* Another frame starts from nothing. */
    m_tracker.on_write(get_id(1) );

    end_frame();

    EXPECT_TRUE(m_loop_carried_id_vec.empty() );
}
