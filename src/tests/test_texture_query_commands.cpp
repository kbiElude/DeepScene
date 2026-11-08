/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Tests for the questions that a .textures file is asked (QueryCommands): what they say of a timeline, and what they say when they cannot answer.
 */
#include <gtest/gtest.h>
#include "analysis/api_dump_textures/query_commands.h"
#include <string>

namespace
{
    typedef Analysis::APIDumpAnalysis::QueryStatus      QueryStatus;
    typedef Analysis::APIDumpTextures::QueryArguments   QueryArguments;
    typedef Analysis::APIDumpTextures::QueryCommands    QueryCommands;
    typedef Analysis::APIDumpTextures::TextureEvent     TextureEvent;
    typedef Analysis::APIDumpTextures::TextureEventKind TextureEventKind;
    typedef Analysis::APIDumpTextures::TextureTimeline  TextureTimeline;

    constexpr uint32_t LEVEL_HEIGHT = 64;
    constexpr uint32_t LEVEL_WIDTH  = 128;

    /* A texture of the window's size that is given a size with no pixels, and filled in a part by a copy that is read in the next frame. */
    TextureTimeline make_timeline()
    {
        TextureTimeline result;

        Analysis::APIDumpAnalysis::Resource resource;

        resource.debug_name = "texture";
        resource.id         = static_cast<Analysis::APIDumpAnalysis::ResourceId>(1);

        result.resource_vec.push_back(resource);

        const auto make_event = [](const TextureEventKind& in_kind,
                                   const uint64_t&         in_frame_index,
                                   const uint32_t&         in_width,
                                   const uint32_t&         in_blob_index)
        {
            TextureEvent event;

            event.blob_index               = in_blob_index;
            event.call_index               = in_frame_index * 100;
            event.destination.resource_id  = static_cast<Analysis::APIDumpAnalysis::ResourceId>(1);
            event.frame_index              = in_frame_index;
            event.height                   = 16;
            event.kind                     = in_kind;
            event.level_format             = Utils::Graphics::TextureFormat::R8G8B8A8_UNORM;
            event.level_height             = LEVEL_HEIGHT;
            event.level_width              = LEVEL_WIDTH;
            event.width                    = in_width;

            return event;
        };

        result.event_vec.push_back(make_event(TextureEventKind::UPLOAD,       0, LEVEL_WIDTH, Analysis::APIDumpTextures::NO_BLOB) );
        result.event_vec.push_back(make_event(TextureEventKind::RENDER_WRITE, 1, 32,          Analysis::APIDumpTextures::NO_BLOB) );
        result.event_vec.push_back(make_event(TextureEventKind::SNAPSHOT,     2, 32,          Analysis::APIDumpTextures::NO_BLOB) );

        return result;
    }

    QueryStatus run(const std::string&     in_command,
                    const QueryArguments&  in_arguments,
                    std::string*           out_text_ptr,
                    std::string*           out_error_ptr)
    {
        Utils::Json::JsonWriter writer(false);

        const TextureTimeline timeline = make_timeline();

        const QueryStatus result = QueryCommands::run(in_command,
                                                      timeline,
                                                      in_arguments,
                                                     &writer,
                                                      out_error_ptr);

        *out_text_ptr = writer.get_text();

        if (result == QueryStatus::OK)
        {
            EXPECT_FALSE(writer.has_failed() );
        }

        return result;
    }

    QueryArguments make_at_arguments(const uint64_t& in_frame_index,
                                     const uint64_t& in_resource_id)
    {
        QueryArguments result;

        result.frame_index     = in_frame_index;
        result.has_frame_index = true;
        result.has_mip_level   = true;
        result.has_resource_id = true;
        result.resource_id     = in_resource_id;

        return result;
    }
}

TEST(TextureQueryCommands, ACommandThatIsNotKnownIsBadArguments)
{
    std::string error;
    std::string text;

    EXPECT_EQ(run("nonsense",
                  QueryArguments(),
                 &text,
                 &error),
              QueryStatus::BAD_ARGUMENTS);
    EXPECT_NE(error.find("Unknown command"),
              std::string::npos);
}

TEST(TextureQueryCommands, InfoCountsTheEventsOfEachKind)
{
    std::string error;
    std::string text;

    ASSERT_EQ(run("info",
                  QueryArguments(),
                 &text,
                 &error),
              QueryStatus::OK);

    EXPECT_NE(text.find("\"n_events\":3"),          std::string::npos);
    EXPECT_NE(text.find("\"n_uploads\":1"),         std::string::npos);
    EXPECT_NE(text.find("\"n_render_writes\":1"),   std::string::npos);
    EXPECT_NE(text.find("\"n_snapshots\":1"),       std::string::npos);
    EXPECT_NE(text.find("\"n_resources_with_events\":1"), std::string::npos);
}

TEST(TextureQueryCommands, ResourcesListsTheTexturesThatHaveEvents)
{
    std::string error;
    std::string text;

    ASSERT_EQ(run("resources",
                  QueryArguments(),
                 &text,
                 &error),
              QueryStatus::OK);

    EXPECT_NE(text.find("\"total\":1"),           std::string::npos);
    EXPECT_NE(text.find("\"resource_id\":1"),     std::string::npos);
    EXPECT_NE(text.find("\"first_frame_index\":0"), std::string::npos);
    EXPECT_NE(text.find("\"last_frame_index\":2"),  std::string::npos);
}

TEST(TextureQueryCommands, ATextureThatTheFileDoesNotHaveIsNotFound)
{
    std::string error;
    std::string text;

    EXPECT_EQ(run("at",
                  make_at_arguments(1,
                                    99),
                 &text,
                 &error),
              QueryStatus::NOT_FOUND);
    EXPECT_NE(error.find("There is no resource 99"),
              std::string::npos);

    QueryArguments resource_arguments;

    resource_arguments.has_resource_id = true;
    resource_arguments.resource_id     = 99;

    EXPECT_EQ(run("resource",
                  resource_arguments,
                 &text,
                 &error),
              QueryStatus::NOT_FOUND);
}

TEST(TextureQueryCommands, AtNeedsAFrameAResourceAndALevel)
{
    std::string error;
    std::string text;

    EXPECT_EQ(run("at",
                  QueryArguments(),
                 &text,
                 &error),
              QueryStatus::BAD_ARGUMENTS);
}

TEST(TextureQueryCommands, AtSaysWhatTheLevelHoldsAndWhichEventMadeIt)
{
    std::string error;
    std::string text;

    ASSERT_EQ(run("at",
                  make_at_arguments(2,
                                    1),
                 &text,
                 &error),
              QueryStatus::OK);

    /* The copy of 32 by 16 texels is all that the level has: the size that it was given with no pixels is not content. */
    EXPECT_NE(text.find("\"covered_texels\":512"),   std::string::npos);
    EXPECT_NE(text.find("\"level_texels\":8192"),    std::string::npos);
    EXPECT_NE(text.find("\"event\":{"),              std::string::npos);
    EXPECT_NE(text.find("\"kind\":\"render_write\""), std::string::npos);
    EXPECT_NE(text.find("\"snapshot\":{"),           std::string::npos);
}

TEST(TextureQueryCommands, AtAnEarlierFrameDoesNotSeeTheLaterEvents)
{
    std::string error;
    std::string text;

    ASSERT_EQ(run("at",
                  make_at_arguments(0,
                                    1),
                 &text,
                 &error),
              QueryStatus::OK);

    EXPECT_NE(text.find("\"covered_texels\":0"),  std::string::npos);
    EXPECT_NE(text.find("\"snapshot\":null"),     std::string::npos);
}
