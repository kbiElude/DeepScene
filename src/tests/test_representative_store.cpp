/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Checks of keeping the detail of a few frames of each shape.
 */
#include <gtest/gtest.h>
#include "analysis/api_dump_analysis/representative_store.h"
#include "frame_detail_marker.h"

TEST(RepresentativeStore, TheFirstFrameOfAShapeIsWanted)
{
    Analysis::APIDumpAnalysis::RepresentativeStore store;

    EXPECT_TRUE(store.wants(0,
                            10) );

    store.add(0,
              5,
              10,
              Tests::FrameDetailMarker::make(1) );

    EXPECT_FALSE(store.wants(0,
                             10) );
    EXPECT_TRUE (store.wants(1,
                             10) );
}

TEST(RepresentativeStore, OnlyTheLightestAndTheHeaviestFramesAreWantedAfterThat)
{
    Analysis::APIDumpAnalysis::RepresentativeStore store;

    store.add(0,
              0,
              10,
              Tests::FrameDetailMarker::make(1) );

    EXPECT_FALSE(store.wants(0,
                             10) );
    EXPECT_TRUE (store.wants(0,
                             9) );
    EXPECT_TRUE (store.wants(0,
                             11) );

    store.add(0,
              3,
              5,
              Tests::FrameDetailMarker::make(2) );
    store.add(0,
              7,
              20,
              Tests::FrameDetailMarker::make(3) );

    EXPECT_FALSE(store.wants(0,
                             5) );
    EXPECT_FALSE(store.wants(0,
                             12) );
    EXPECT_FALSE(store.wants(0,
                             20) );
    EXPECT_TRUE (store.wants(0,
                             4) );
    EXPECT_TRUE (store.wants(0,
                             21) );
}

TEST(RepresentativeStore, EntriesAreTheFirstTheLightestAndTheHeaviest)
{
    Analysis::APIDumpAnalysis::RepresentativeStore store;

    store.add(0,
              0,
              10,
              Tests::FrameDetailMarker::make(1) );
    store.add(0,
              3,
              5,
              Tests::FrameDetailMarker::make(2) );
    store.add(0,
              7,
              20,
              Tests::FrameDetailMarker::make(3) );

    const auto entry_ptr_vec = store.get_entry_ptr_vec(0);

    ASSERT_EQ(entry_ptr_vec.size(),
              3u);

    EXPECT_EQ(Tests::FrameDetailMarker::read(entry_ptr_vec.at(0)->detail),
              1u);
    EXPECT_EQ(entry_ptr_vec.at(0)->frame_index,
              0u);
    EXPECT_EQ(Tests::FrameDetailMarker::read(entry_ptr_vec.at(1)->detail),
              2u);
    EXPECT_EQ(entry_ptr_vec.at(1)->weight,
              5u);
    EXPECT_EQ(Tests::FrameDetailMarker::read(entry_ptr_vec.at(2)->detail),
              3u);
    EXPECT_EQ(entry_ptr_vec.at(2)->frame_index,
              7u);
}

TEST(RepresentativeStore, ALoneFrameIsItsOwnLightestAndHeaviest)
{
    Analysis::APIDumpAnalysis::RepresentativeStore store;

    store.add(4,
              9,
              3,
              Tests::FrameDetailMarker::make(1) );

    EXPECT_EQ(store.get_entry_ptr_vec(4).size(),
              1u);
    EXPECT_TRUE(store.get_entry_ptr_vec(5).empty() );
}

TEST(RepresentativeStore, AMillionFramesKeepAtMostThreePerShape)
{
    constexpr uint32_t N_FRAMES = 1000000;
    constexpr uint32_t N_SHAPES = 3;

    Analysis::APIDumpAnalysis::RepresentativeStore store;
    uint64_t                                       n_wanted     = 0;
    uint64_t                                       random_state = 12345;

    for (uint32_t n_frame = 0;
                  n_frame < N_FRAMES;
                ++n_frame)
    {
        const uint32_t shape_id = n_frame % N_SHAPES;

        /* A linear congruential generator, for weights which are not in any order. */
        random_state = random_state * 6364136223846793005ull + 1442695040888963407ull;

        const uint64_t weight = (random_state >> 33) % 1000;

        if (store.wants(shape_id,
                        weight) )
        {
            store.add(shape_id,
                      n_frame,
                      weight,
                      Tests::FrameDetailMarker::make(n_frame) );

            n_wanted++;
        }
    }

    for (uint32_t n_shape = 0;
                  n_shape < N_SHAPES;
                ++n_shape)
    {
        EXPECT_LE(store.get_entry_ptr_vec(n_shape).size(),
                  3u);
    }

    /* The detail was built for few frames only. */
    EXPECT_LT(n_wanted,
              static_cast<uint64_t>(N_FRAMES) / 100);
}
