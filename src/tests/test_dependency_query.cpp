/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Checks of what the .deps file is asked: each question is answered with the documented JSON and the right numbers, and a question with no
 * answer is told so.
 */
#include <gtest/gtest.h>
#include "analysis/api_dump_analysis/json_reader.h"
#include "analysis/api_dump_dependencies/query_commands.h"
#include "dependency_scene.h"

namespace
{
    using namespace Analysis::APIDumpAnalysis;
    using namespace Analysis::APIDumpDependencies;
}

class DependencyQueryTest : public ::testing::Test
{
protected:
    /* Runs a command, and reads its JSON back. */
    QueryStatus ask(const std::string& in_command,
                    JsonValue*         out_value_ptr)
    {
        JsonWriter  writer(true); /* in_is_pretty */
        const auto  status = QueryCommands::run(in_command,
                                                m_analysis,
                                                m_arguments,
                                               &writer,
                                               &m_error);

        if (status == QueryStatus::OK)
        {
            EXPECT_FALSE(writer.has_failed() );
            EXPECT_TRUE (JsonReader::parse(writer.get_text(),
                                           out_value_ptr,
                                           nullptr) ) << writer.get_text();
        }

        return status;
    }

    /* Five frames, and resources of every kind:
     *
     *   1  a texture which is uploaded and then sampled            (static)
     *   2  a target which is cleared and sampled in each frame     (frame local)
     *   3  a target which is drawn to and sampled                  (carried, possibly)
     *   4  a target which is sampled, cleared and drawn to         (carried)
     *   5  the swap chain image, which is blended into             (carried)
     *
     * 3 and 4 are made in the same way, and 4 is the second.
     */
    void SetUp() override
    {
        Tests::DependencyScene scene;

        const auto texture = scene.create_resource(1,
                                                   ResourceKind::TEXTURE_2D,
                                                   false,
                                                   0xAA);
        const auto cleared = scene.create_resource(1,
                                                   ResourceKind::TEXTURE_2D,
                                                   true,
                                                   0xBB);
        const auto drawn   = scene.create_resource(1,
                                                   ResourceKind::TEXTURE_2D,
                                                   true,
                                                   0xCC);
        const auto feeding = scene.create_resource(1,
                                                   ResourceKind::TEXTURE_2D,
                                                   true,
                                                   0xCC);
        const auto image   = scene.create_resource(1,
                                                   ResourceKind::SWAP_CHAIN_IMAGE,
                                                   true,
                                                   0xDD);

        for (uint32_t n_frame = 0;
                      n_frame < 5;
                    ++n_frame)
        {
            if (n_frame == 0)
            {
                scene.access   (texture,
                                AccessRole::UPLOAD_DESTINATION,
                                AccessMode::WRITE,
                                AccessCoverage::FULL);
                scene.next_call();
            }
            else
            {
                scene.sample(texture);
            }

            scene.clear_fully(cleared);
            scene.sample     (cleared);

            scene.draw_to(drawn);
            scene.sample (drawn);

            scene.sample     (feeding);
            scene.clear_fully(feeding);
            scene.draw_to    (feeding);

            scene.access   (image,
                            AccessRole::COLOR_ATTACHMENT,
                            AccessMode::READ_WRITE);
            scene.next_call();

            scene.end_frame();
        }

        m_analysis = scene.make_analysis();
    }

    DependencyAnalysis                                m_analysis;
    Analysis::APIDumpDependencies::QueryArguments     m_arguments;
    std::string                                       m_error;
};

TEST_F(DependencyQueryTest, InfoSaysWhatTheFileIsOfAndHowManyResourcesAreOfEachKind)
{
    JsonValue value;

    ASSERT_EQ(ask("info",
                 &value),
              QueryStatus::OK);

    EXPECT_EQ(value.at("workload").at("filename").string_value,
              "x.workload");
    EXPECT_EQ(value.at("summary").at("n_frames").number_value,
              5.0);
    EXPECT_EQ(value.at("summary").at("n_resources").number_value,
              5.0);
    EXPECT_EQ(value.at("resources_by_class").at("carried").number_value,
              2.0);
    EXPECT_EQ(value.at("resources_by_class").at("carried_possibly").number_value,
              1.0);
    EXPECT_EQ(value.at("resources_by_class").at("frame_local").number_value,
              1.0);
    EXPECT_EQ(value.at("resources_by_class").at("static").number_value,
              1.0);
    EXPECT_EQ(value.at("resources_by_class").at("unused").number_value,
              0.0);
}

TEST_F(DependencyQueryTest, ResourcesCanBeChosenByClassAndPaged)
{
    JsonValue value;

    ASSERT_EQ(ask("resources",
                 &value),
              QueryStatus::OK);
    EXPECT_EQ(value.at("resources").at("total").number_value,
              5.0);

    m_arguments.dependency_class = DependencyClass::CARRIED;
    m_arguments.has_class        = true;

    ASSERT_EQ(ask("resources",
                 &value),
              QueryStatus::OK);
    EXPECT_EQ(value.at("resources").at("total").number_value,
              2.0);
    ASSERT_EQ(value.at("resources").at("items").item_vec.size(),
              2u);
    EXPECT_EQ(value.at("resources").at("items").item_vec.at(0).at("resource_id").number_value,
              4.0);
    EXPECT_EQ(value.at("resources").at("items").item_vec.at(0).at("class").string_value,
              "carried");

    m_arguments.limit = 1;

    ASSERT_EQ(ask("resources",
                 &value),
              QueryStatus::OK);
    EXPECT_EQ(value.at("resources").at("total").number_value,
              2.0);
    EXPECT_EQ(value.at("resources").at("items").item_vec.size(),
              1u);
}

TEST_F(DependencyQueryTest, AResourceIsToldByItsIdentityItsRunsAndItsVersions)
{
    JsonValue value;

    m_arguments.has_resource_id = true;
    m_arguments.resource_id     = 4;

    ASSERT_EQ(ask("resource",
                 &value),
              QueryStatus::OK);

    /* The second made in the way, by its key and how many were made so before it. */
    EXPECT_EQ(value.at("name").string_value,
              "Surface#3");
    EXPECT_EQ(value.at("description_key").string_value,
              "0x00000000000000cc");
    EXPECT_EQ(value.at("ordinal").number_value,
              1.0);
    EXPECT_EQ(value.at("class").string_value,
              "carried");
    EXPECT_EQ(value.at("summary").at("n_frames_carrying").number_value,
              5.0);

    ASSERT_EQ(value.at("runs").at("items").item_vec.size(),
              1u);
    EXPECT_EQ(value.at("runs").at("items").item_vec.at(0).at("carry").string_value,
              "certain");

    /* What the application made it with, and what each frame wrote. */
    ASSERT_EQ(value.at("versions").at("items").item_vec.size(),
              6u);
    EXPECT_EQ(value.at("versions").at("items").item_vec.at(0).at("writer_frame_index").type,
              JsonValue::Type::NULL_VALUE);
    EXPECT_EQ(value.at("versions").at("items").item_vec.at(3).at("writer_frame_index").number_value,
              2.0);
}

TEST_F(DependencyQueryTest, AResourceWhichIsNotThereIsNotFound)
{
    JsonValue value;

    EXPECT_EQ(ask("resource",
                 &value),
              QueryStatus::BAD_ARGUMENTS);

    m_arguments.has_resource_id = true;
    m_arguments.resource_id     = 99;

    EXPECT_EQ(ask("resource",
                 &value),
              QueryStatus::NOT_FOUND);

    m_arguments.resource_id = 0;

    EXPECT_EQ(ask("resource",
                 &value),
              QueryStatus::NOT_FOUND);
}

TEST_F(DependencyQueryTest, AFrameTellsWhatItDoesWithWhatItTouchesAndWhichFrameLeftWhatItUses)
{
    JsonValue value;

    m_arguments.frame_index     = 3;
    m_arguments.has_frame_index = true;

    ASSERT_EQ(ask("frame",
                 &value),
              QueryStatus::OK);

    /* Ten calls to a frame: nine, and the one that ends it. */
    EXPECT_EQ(value.at("calls").at("first_call_index").number_value,
              30.0);
    EXPECT_NE(value.at("calls").at("printer_command").string_value.find("--frame 3"),
              std::string::npos);

    const auto& item_vec = value.at("resources").at("items").item_vec;

    /* Every resource is touched in every frame. */
    ASSERT_EQ(item_vec.size(),
              5u);

    /* The resource which is sampled before it is cleared uses what the frame before wrote. */
    const JsonValue& feeding = item_vec.at(3);

    EXPECT_EQ(feeding.at("resource_id").number_value,
              4.0);
    EXPECT_EQ(feeding.at("carry").string_value,
              "certain");
    EXPECT_TRUE(feeding.at("is_written").bool_value);
    EXPECT_EQ(feeding.at("content_of").at("writer_frame_index").number_value,
              2.0);
    EXPECT_EQ(feeding.at("content_of").at("distance").number_value,
              1.0);

    /* One which is cleared first uses nothing. */
    EXPECT_EQ(item_vec.at(1).at("carry").string_value,
              "none");
    EXPECT_EQ(item_vec.at(1).find_ptr("content_of"),
              nullptr);

    EXPECT_TRUE(item_vec.at(1).at("is_fully_written").bool_value);
}

TEST_F(DependencyQueryTest, CarriesLeavesOutWhatAFrameDoesNotUse)
{
    JsonValue value;

    m_arguments.frame_index     = 3;
    m_arguments.has_frame_index = true;

    ASSERT_EQ(ask("carries",
                 &value),
              QueryStatus::OK);

    /* All but the one which is cleared first. */
    ASSERT_EQ(value.at("resources").at("total").number_value,
              4.0);

    for (const auto& item : value.at("resources").at("items").item_vec)
    {
        EXPECT_NE(item.at("carry").string_value,
                  "none");
        EXPECT_NE(item.at("resource_id").number_value,
                  2.0);
    }

    /* The texture which was uploaded in the first frame. */
    EXPECT_EQ(value.at("resources").at("items").item_vec.at(0).at("content_of").at("writer_frame_index").number_value,
              0.0);
    EXPECT_EQ(value.at("resources").at("items").item_vec.at(0).at("content_of").at("distance").number_value,
              3.0);
}

TEST_F(DependencyQueryTest, AFrameWhichIsNotThereIsNotFound)
{
    JsonValue value;

    EXPECT_EQ(ask("frame",
                 &value),
              QueryStatus::BAD_ARGUMENTS);

    m_arguments.frame_index     = 5;
    m_arguments.has_frame_index = true;

    EXPECT_EQ(ask("frame",
                 &value),
              QueryStatus::NOT_FOUND);
    EXPECT_EQ(ask("carries",
                 &value),
              QueryStatus::NOT_FOUND);
}

TEST_F(DependencyQueryTest, PerEyeNamesTheTargetsWhichCarryContentAsARecipeDoes)
{
    JsonValue value;

    ASSERT_EQ(ask("per-eye",
                 &value),
              QueryStatus::OK);

    const auto& candidate_vec = value.at("candidates").item_vec;

    /* The two made in the same way; not the target which is cleared, the texture, or the image of the swap chain. */
    ASSERT_EQ(candidate_vec.size(),
              2u);
    EXPECT_EQ(candidate_vec.at(0).at("resource_id").number_value,
              3.0);
    EXPECT_EQ(candidate_vec.at(0).at("description_key").string_value,
              "0x00000000000000cc");
    EXPECT_EQ(candidate_vec.at(0).at("ordinal").number_value,
              0.0);
    EXPECT_EQ(candidate_vec.at(0).at("certainty").string_value,
              "possible");
    EXPECT_EQ(candidate_vec.at(1).at("resource_id").number_value,
              4.0);
    EXPECT_EQ(candidate_vec.at(1).at("ordinal").number_value,
              1.0);
    EXPECT_EQ(candidate_vec.at(1).at("certainty").string_value,
              "certain");
}

TEST_F(DependencyQueryTest, ACommandWhichIsNotOneIsRefused)
{
    JsonValue value;

    EXPECT_EQ(ask("nonsense",
                 &value),
              QueryStatus::BAD_ARGUMENTS);
    EXPECT_FALSE(m_error.empty() );

    for (const auto& command : QueryCommands::get_command_names() )
    {
        EXPECT_NE(command,
                  "nonsense");
    }
}

TEST_F(DependencyQueryTest, AClassIsReadByItsName)
{
    DependencyClass dependency_class = DependencyClass::UNUSED;

    EXPECT_TRUE(QueryCommands::parse_dependency_class("carried_possibly",
                                                     &dependency_class) );
    EXPECT_EQ  (dependency_class,
                DependencyClass::CARRIED_POSSIBLY);
    EXPECT_FALSE(QueryCommands::parse_dependency_class("loop_carried",
                                                      &dependency_class) );
}
