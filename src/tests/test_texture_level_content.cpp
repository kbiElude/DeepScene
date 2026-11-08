/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Tests for TextureLevelContent: what a level holds at a point, from the events that write it laid over each other in the order of the calls.
 */
#include <gtest/gtest.h>
#include "analysis/api_dump_textures/texture_level_content.h"

namespace
{
    typedef Analysis::APIDumpTextures::LevelContent        LevelContent;
    typedef Analysis::APIDumpTextures::LevelCut            LevelCut;
    typedef Analysis::APIDumpTextures::TextureEvent        TextureEvent;
    typedef Analysis::APIDumpTextures::TextureEventKind    TextureEventKind;
    typedef Analysis::APIDumpTextures::TextureLevelContent TextureLevelContent;
    typedef Analysis::APIDumpTextures::TextureTimeline     TextureTimeline;

    constexpr uint32_t LEVEL_HEIGHT = 64;
    constexpr uint32_t LEVEL_WIDTH  = 128;

    const Analysis::APIDumpAnalysis::ResourceId RESOURCE_ID = static_cast<Analysis::APIDumpAnalysis::ResourceId>(3);

    /* The texels that the pieces cover (each is its own: the pieces do not overlap). */
    uint64_t count_texels(const LevelContent& in_content)
    {
        uint64_t result = 0;

        for (const auto& piece : in_content.piece_vec)
        {
            result += static_cast<uint64_t>(piece.width) * piece.height * piece.layer_count;
        }

        return result;
    }

    /* An event over a region of a level of the texture; a layer count above 1 is a texture with that many layers. */
    TextureEvent make_event(const TextureEventKind& in_kind,
                            const uint64_t&         in_frame_index,
                            const uint64_t&         in_call_index,
                            const uint32_t&         in_x,
                            const uint32_t&         in_y,
                            const uint32_t&         in_width,
                            const uint32_t&         in_height,
                            const bool&             in_has_data = true,
                            const uint32_t&         in_mip_level = 0,
                            const uint32_t&         in_first_layer = 0,
                            const uint32_t&         in_layer_count = 1,
                            const uint32_t&         in_level_layer_count = 1,
                            const uint32_t&         in_level_width = LEVEL_WIDTH)
    {
        TextureEvent result;

        result.blob_index                  = (in_has_data) ? 0
                                                           : Analysis::APIDumpTextures::NO_BLOB;
        result.call_index                  = in_call_index;
        result.destination.layer           = in_first_layer;
        result.destination.mip_level       = in_mip_level;
        result.destination.resource_id     = RESOURCE_ID;
        result.frame_index                 = in_frame_index;
        result.height                      = in_height;
        result.kind                        = in_kind;
        result.layer_count                 = in_layer_count;
        result.level_depth_or_layer_count  = in_level_layer_count;
        result.level_format                = Utils::Graphics::TextureFormat::R8G8B8A8_UNORM;
        result.level_height                = LEVEL_HEIGHT;
        result.level_width                 = in_level_width;
        result.width                       = in_width;
        result.x                           = in_x;
        result.y                           = in_y;

        return result;
    }

    LevelContent find(const TextureTimeline& in_timeline,
                      const LevelCut&        in_cut = LevelCut() )
    {
        return TextureLevelContent::find(in_timeline,
                                         RESOURCE_ID,
                                         0,
                                         in_cut);
    }
}

TEST(TextureLevelContent, ALevelThatNoEventNamesIsNotThere)
{
    TextureTimeline timeline;

    const LevelContent content = find(timeline);

    EXPECT_FALSE(content.has_level);
    EXPECT_TRUE (content.piece_vec.empty() );
}

TEST(TextureLevelContent, AnUploadWithPixelsIsAPieceOfItsRegion)
{
    TextureTimeline timeline;

    timeline.event_vec.push_back(make_event(TextureEventKind::UPLOAD, 0, 10, 0, 0, LEVEL_WIDTH, LEVEL_HEIGHT) );

    const LevelContent content = find(timeline);

    ASSERT_EQ(content.piece_vec.size(),
              1u);
    EXPECT_TRUE(content.has_level);
    EXPECT_EQ  (content.level_width,
                LEVEL_WIDTH);
    EXPECT_EQ  (content.piece_vec.at(0).event_index,
                0u);
    EXPECT_EQ  (count_texels(content),
                static_cast<uint64_t>(LEVEL_WIDTH) * LEVEL_HEIGHT);
}

TEST(TextureLevelContent, AnUploadWithNoPixelsGivesTheLevelASizeAndWritesNothing)
{
    TextureTimeline timeline;

    timeline.event_vec.push_back(make_event(TextureEventKind::UPLOAD, 0, 10, 0, 0, LEVEL_WIDTH, LEVEL_HEIGHT, false /* in_has_data */) );

    const LevelContent content = find(timeline);

    EXPECT_TRUE(content.has_level);
    EXPECT_EQ  (content.level_height,
                LEVEL_HEIGHT);
    EXPECT_TRUE(content.piece_vec.empty() );
}

TEST(TextureLevelContent, AWriteThatCoversPartOfAnEarlierOneLeavesTheRestOfIt)
{
    TextureTimeline timeline;

    timeline.event_vec.push_back(make_event(TextureEventKind::UPLOAD,       0, 10, 0,  0,  100, 40) );
    timeline.event_vec.push_back(make_event(TextureEventKind::RENDER_WRITE, 0, 20, 30, 10, 20,  10) );

    const LevelContent content = find(timeline);

    /* The texels of both, with those that the second covers counted once. */
    EXPECT_EQ(count_texels(content),
              static_cast<uint64_t>(100) * 40);

    uint64_t n_texels_of_second = 0;

    for (const auto& piece : content.piece_vec)
    {
        if (piece.event_index == 1)
        {
            n_texels_of_second += static_cast<uint64_t>(piece.width) * piece.height;
        }
    }

    EXPECT_EQ(n_texels_of_second,
              static_cast<uint64_t>(20) * 10);
}

TEST(TextureLevelContent, AWriteThatCoversAnEarlierOneLeavesNothingOfIt)
{
    TextureTimeline timeline;

    timeline.event_vec.push_back(make_event(TextureEventKind::UPLOAD,       0, 10, 10, 10, 20, 20) );
    timeline.event_vec.push_back(make_event(TextureEventKind::RENDER_WRITE, 0, 20,  0,  0, 40, 40) );

    const LevelContent content = find(timeline);

    ASSERT_EQ(content.piece_vec.size(),
              1u);
    EXPECT_EQ(content.piece_vec.at(0).event_index,
              1u);
}

TEST(TextureLevelContent, TheEventsAfterTheCutAreNotCounted)
{
    TextureTimeline timeline;

    timeline.event_vec.push_back(make_event(TextureEventKind::UPLOAD,       0, 10, 0, 0, 20, 20) );
    timeline.event_vec.push_back(make_event(TextureEventKind::RENDER_WRITE, 1, 20, 0, 0, 40, 40) );
    timeline.event_vec.push_back(make_event(TextureEventKind::RENDER_WRITE, 1, 30, 0, 0, 60, 60) );

    LevelCut cut_at_frame;
    LevelCut cut_at_call;

    cut_at_frame.last_frame_index = 0;
    cut_at_call.last_call_index   = 20;

    EXPECT_EQ(count_texels(find(timeline,
                                cut_at_frame) ),
              static_cast<uint64_t>(20) * 20);
    EXPECT_EQ(count_texels(find(timeline,
                                cut_at_call) ),
              static_cast<uint64_t>(40) * 40);
    EXPECT_EQ(count_texels(find(timeline) ),
              static_cast<uint64_t>(60) * 60);
}

TEST(TextureLevelContent, ALevelThatIsGivenAnotherSizeStartsAgain)
{
    TextureTimeline timeline;

    timeline.event_vec.push_back(make_event(TextureEventKind::UPLOAD, 0, 10, 0, 0, LEVEL_WIDTH,     LEVEL_HEIGHT) );
    timeline.event_vec.push_back(make_event(TextureEventKind::UPLOAD, 0, 20, 0, 0, LEVEL_WIDTH / 2, 8,
                                            true /* in_has_data */, 0, 0, 1, 1,
                                            LEVEL_WIDTH / 2) );

    const LevelContent content = find(timeline);

    ASSERT_EQ(content.piece_vec.size(),
              1u);
    EXPECT_EQ(content.piece_vec.at(0).event_index,
              1u);
    EXPECT_EQ(content.level_width,
              LEVEL_WIDTH / 2);
}

TEST(TextureLevelContent, ASizeGivenAgainWithNoPixelsEmptiesTheLevelButAPartDoesNot)
{
    TextureTimeline timeline;

    timeline.event_vec.push_back(make_event(TextureEventKind::UPLOAD, 0, 10, 0, 0, 20, 20) );
    timeline.event_vec.push_back(make_event(TextureEventKind::UPLOAD, 0, 20, 0, 0, 4,  4, false /* in_has_data */) );

    /* A part with no pixels changes nothing. */
    EXPECT_EQ(count_texels(find(timeline) ),
              static_cast<uint64_t>(20) * 20);

    timeline.event_vec.push_back(make_event(TextureEventKind::UPLOAD, 0, 30, 0, 0, LEVEL_WIDTH, LEVEL_HEIGHT, false /* in_has_data */) );

    /* The whole level with no pixels: what it held is not known. */
    EXPECT_EQ(count_texels(find(timeline) ),
              0u);
}

TEST(TextureLevelContent, LayersThatHoldTheSameAreOneRun)
{
    TextureTimeline timeline;

    timeline.event_vec.push_back(make_event(TextureEventKind::UPLOAD, 0, 10, 0, 0, 16, 16, true /* in_has_data */, 0, 0, 3, 3) );

    const LevelContent content = find(timeline);

    ASSERT_EQ(content.piece_vec.size(),
              1u);
    EXPECT_EQ(content.piece_vec.at(0).first_layer,
              0u);
    EXPECT_EQ(content.piece_vec.at(0).layer_count,
              3u);

    /* A write to the middle layer splits the run: the layers on its sides are alike, but are not next to each other. */
    timeline.event_vec.push_back(make_event(TextureEventKind::RENDER_WRITE, 0, 20, 0, 0, 8, 8, true /* in_has_data */, 0, 1, 1, 3) );

    const LevelContent split_content = find(timeline);

    uint64_t n_texels_of_first = 0;

    for (const auto& piece : split_content.piece_vec)
    {
        if (piece.event_index == 0)
        {
            n_texels_of_first += static_cast<uint64_t>(piece.width) * piece.height * piece.layer_count;
        }
    }

    /* Three layers of 16 by 16, less the 8 by 8 that the write covers. */
    EXPECT_GE(split_content.piece_vec.size(),
              3u);
    EXPECT_EQ(n_texels_of_first,
              static_cast<uint64_t>(3) * 16 * 16 - 8 * 8);
}

TEST(TextureLevelContent, ASnapshotIsReportedAndWritesNothing)
{
    TextureTimeline timeline;

    timeline.event_vec.push_back(make_event(TextureEventKind::RENDER_WRITE, 0, 10, 0, 0, 20, 20) );
    timeline.event_vec.push_back(make_event(TextureEventKind::SNAPSHOT,     1, 20, 0, 0, 20, 20) );

    const LevelContent content = find(timeline);

    EXPECT_TRUE(content.has_snapshot);
    EXPECT_EQ  (content.snapshot_event_index,
                1u);
    ASSERT_EQ  (content.piece_vec.size(),
                1u);
    EXPECT_EQ  (content.piece_vec.at(0).event_index,
                0u);
}

TEST(TextureLevelContent, OtherLevelsAndOtherResourcesAreNotThisOnes)
{
    TextureTimeline timeline;

    timeline.event_vec.push_back(make_event(TextureEventKind::UPLOAD, 0, 10, 0, 0, 20, 20, true /* in_has_data */, 1) );

    TextureEvent other_resource = make_event(TextureEventKind::UPLOAD, 0, 20, 0, 0, 20, 20);

    other_resource.destination.resource_id = static_cast<Analysis::APIDumpAnalysis::ResourceId>(4);

    timeline.event_vec.push_back(other_resource);

    EXPECT_FALSE(find(timeline).has_level);
}
