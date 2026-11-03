/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Checks of what the model builder tells of the use of resources (see IAccessSink): what is read and written, in which order, how much of it a
 * write reaches, and where the frames end.
 */
#include <gtest/gtest.h>
#include "Analysis/APIDumpAnalysis/access_sink.h"
#include "model_test_helper.h"
#include <vector>

namespace
{
    class RecordingSink : public Analysis::APIDumpAnalysis::IAccessSink
    {
    public:
        void on_access(const Analysis::APIDumpAnalysis::AccessEvent& in_event) override
        {
            event_vec.push_back(in_event);
        }

        void on_frame_ended(const uint64_t& in_frame_index) override
        {
            ended_frame_index_vec.push_back(in_frame_index);
        }

        /* The events which are of the resource, in order. */
        std::vector<Analysis::APIDumpAnalysis::AccessEvent> find_events(const Analysis::APIDumpAnalysis::ResourceId& in_id) const
        {
            std::vector<Analysis::APIDumpAnalysis::AccessEvent> result_vec;

            for (const auto& event : event_vec)
            {
                if (event.resource_id == in_id)
                {
                    result_vec.push_back(event);
                }
            }

            return result_vec;
        }

        std::vector<uint64_t>                               ended_frame_index_vec;
        std::vector<Analysis::APIDumpAnalysis::AccessEvent> event_vec;
    };
}

TEST(AccessEvents, ADrawReadsTheTextureAndWritesTheTarget)
{
    Tests::ModelTestHelper helper;
    RecordingSink          sink;

    helper.get_builder().set_access_sink(&sink);

    const auto target  = helper.create_color_texture();
    const auto texture = helper.create_static_texture();

    helper.bind(target);
    helper.draw_textured(texture);

    const auto texture_event_vec = sink.find_events(texture);
    const auto target_event_vec  = sink.find_events(target);

    ASSERT_EQ(texture_event_vec.size(),
              1u);
    EXPECT_EQ(texture_event_vec.at(0).role,
              Analysis::APIDumpAnalysis::AccessRole::SAMPLED);
    EXPECT_EQ(texture_event_vec.at(0).mode,
              Analysis::APIDumpAnalysis::AccessMode::READ);

    ASSERT_EQ(target_event_vec.size(),
              1u);
    EXPECT_EQ(target_event_vec.at(0).role,
              Analysis::APIDumpAnalysis::AccessRole::COLOR_ATTACHMENT);
    EXPECT_EQ(target_event_vec.at(0).mode,
              Analysis::APIDumpAnalysis::AccessMode::WRITE);

    /* How much of the target a draw reaches is not told: it is for the analysis to be careful with. */
    EXPECT_EQ(target_event_vec.at(0).coverage,
              Analysis::APIDumpAnalysis::AccessCoverage::UNKNOWN);

    /* The reads come before the writes. */
    ASSERT_GE(sink.event_vec.size(),
              2u);
    EXPECT_EQ(sink.event_vec.front().resource_id,
              texture);
    EXPECT_EQ(sink.event_vec.back().resource_id,
              target);
}

TEST(AccessEvents, ADrawWhichBlendsOrTestsDepthReadsWhatItWritesTo)
{
    Tests::ModelTestHelper helper;
    RecordingSink          sink;

    helper.get_builder().set_access_sink(&sink);

    const auto color = helper.create_color_texture();
    const auto depth = helper.create_depth_buffer();

    helper.bind(color,
                depth);
    helper.draw_reading_attachments();

    ASSERT_EQ(sink.find_events(color).size(),
              1u);
    EXPECT_EQ(sink.find_events(color).at(0).mode,
              Analysis::APIDumpAnalysis::AccessMode::READ_WRITE);

    ASSERT_EQ(sink.find_events(depth).size(),
              1u);
    EXPECT_EQ(sink.find_events(depth).at(0).role,
              Analysis::APIDumpAnalysis::AccessRole::DEPTH_STENCIL_ATTACHMENT);
    EXPECT_EQ(sink.find_events(depth).at(0).mode,
              Analysis::APIDumpAnalysis::AccessMode::READ_WRITE);
}

TEST(AccessEvents, ATextureWhichIsAlsoWhatIsRenderedToIsReadAndWrittenByTheSameCall)
{
    Tests::ModelTestHelper helper;
    RecordingSink          sink;

    helper.get_builder().set_access_sink(&sink);

    const auto target = helper.create_color_texture();

    helper.bind(target);
    helper.draw_textured(target);

    const auto event_vec = sink.find_events(target);

    ASSERT_EQ(event_vec.size(),
              2u);
    EXPECT_EQ(event_vec.at(0).role,
              Analysis::APIDumpAnalysis::AccessRole::SAMPLED);
    EXPECT_EQ(event_vec.at(1).role,
              Analysis::APIDumpAnalysis::AccessRole::COLOR_ATTACHMENT);
    EXPECT_EQ(event_vec.at(0).call_index,
              event_vec.at(1).call_index);
}

TEST(AccessEvents, AClearOfTheWholeViewportOfATargetIsFullButOfLessOfItIsNot)
{
    Tests::ModelTestHelper helper;
    RecordingSink          sink;

    helper.get_builder().set_access_sink(&sink);

    const auto target = helper.create_color_texture(); /* 512 x 512 */

    helper.bind(target);

    Analysis::APIDumpAnalysis::ClearOperation clear;

    clear.clears_color           = true;
    clear.is_limited_to_viewport = true;

    helper.set_viewport(0, 0, 512, 512);
    helper.clear       (clear);
    helper.set_viewport(0, 40, 512, 400);
    helper.clear       (clear);

    const auto event_vec = sink.find_events(target);

    ASSERT_EQ(event_vec.size(),
              2u);
    EXPECT_EQ(event_vec.at(0).role,
              Analysis::APIDumpAnalysis::AccessRole::CLEAR_TARGET);
    EXPECT_EQ(event_vec.at(0).mode,
              Analysis::APIDumpAnalysis::AccessMode::WRITE);
    EXPECT_EQ(event_vec.at(0).coverage,
              Analysis::APIDumpAnalysis::AccessCoverage::FULL);
    EXPECT_EQ(event_vec.at(1).coverage,
              Analysis::APIDumpAnalysis::AccessCoverage::PARTIAL);
}

TEST(AccessEvents, AClearOfRectanglesIsPartialAndOneWhichIsNotOfTheViewportIsFull)
{
    Tests::ModelTestHelper helper;
    RecordingSink          sink;

    helper.get_builder().set_access_sink(&sink);

    const auto target = helper.create_color_texture();

    helper.bind(target);
    helper.set_viewport(0, 0, 16, 16); /* far less than the target */

    Analysis::APIDumpAnalysis::ClearOperation rectangles;
    Analysis::APIDumpAnalysis::ClearOperation whole;

    rectangles.clears_color   = true;
    rectangles.has_rectangles = true;
    whole.clears_color        = true;

    helper.clear(rectangles);
    helper.clear(whole);

    const auto event_vec = sink.find_events(target);

    ASSERT_EQ(event_vec.size(),
              2u);
    EXPECT_EQ(event_vec.at(0).coverage,
              Analysis::APIDumpAnalysis::AccessCoverage::PARTIAL);
    EXPECT_EQ(event_vec.at(1).coverage,
              Analysis::APIDumpAnalysis::AccessCoverage::FULL);
}

TEST(AccessEvents, ACopyReadsTheSourceAndWritesTheDestinationAndAnUploadWrites)
{
    Tests::ModelTestHelper helper;
    RecordingSink          sink;

    helper.get_builder().set_access_sink(&sink);

    const auto destination = helper.create_color_texture();
    const auto source      = helper.create_static_texture();

    helper.copy  (source,
                  destination);
    helper.upload(destination);

    ASSERT_EQ(sink.find_events(source).size(),
              1u);
    EXPECT_EQ(sink.find_events(source).at(0).role,
              Analysis::APIDumpAnalysis::AccessRole::TRANSFER_SOURCE);
    EXPECT_EQ(sink.find_events(source).at(0).mode,
              Analysis::APIDumpAnalysis::AccessMode::READ);

    const auto destination_event_vec = sink.find_events(destination);

    ASSERT_EQ(destination_event_vec.size(),
              2u);
    EXPECT_EQ(destination_event_vec.at(0).role,
              Analysis::APIDumpAnalysis::AccessRole::TRANSFER_DESTINATION);
    EXPECT_EQ(destination_event_vec.at(1).role,
              Analysis::APIDumpAnalysis::AccessRole::UPLOAD_DESTINATION);
    EXPECT_EQ(destination_event_vec.at(1).mode,
              Analysis::APIDumpAnalysis::AccessMode::WRITE);
}

TEST(AccessEvents, TheFramesAreToldWhereTheyEndInOrder)
{
    Tests::ModelTestHelper helper;
    RecordingSink          sink;

    helper.get_builder().set_access_sink(&sink);

    helper.end_frame();
    helper.end_frame();
    helper.end_frame();

    ASSERT_EQ(sink.ended_frame_index_vec.size(),
              3u);
    EXPECT_EQ(sink.ended_frame_index_vec.at(0),
              0u);
    EXPECT_EQ(sink.ended_frame_index_vec.at(2),
              2u);
}

TEST(AccessEvents, AModelBuilderWithoutASinkWorksAsItDid)
{
    Tests::ModelTestHelper helper;

    const auto target = helper.create_color_texture();

    helper.bind(target);
    helper.draw();
    helper.end_frame();

    EXPECT_EQ(helper.get_model().frame_record_vec.size(),
              1u);
}
