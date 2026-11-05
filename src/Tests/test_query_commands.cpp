/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Tests for the query commands on the analysis of the fr08 slice: each answers with the documented JSON and the right numbers,
 * and a question with no answer is reported.
 */
#include <gtest/gtest.h>
#include "Analysis/APIDumpAnalysis/depth_histogram.h"
#include "Analysis/APIDumpAnalysis/frontends/analysis_session.h"
#include "Analysis/APIDumpAnalysis/json_reader.h"
#include "Analysis/APIDumpAnalysis/key_builder.h"
#include "Analysis/APIDumpAnalysis/query_commands.h"
#include "Analysis/APIDumpAnalysis/resource_access_tracker.h"
#include "data_fixtures.h"
#include <iterator>
#include <set>
#include <stdio.h>

class QueryCommandsTest : public ::testing::Test
{
protected:
    /* Runs a command, and reads its JSON back. */
    Analysis::APIDumpAnalysis::QueryStatus ask(const std::string& in_command,
                                               Analysis::APIDumpAnalysis::JsonValue*  out_value_ptr)
    {
        Analysis::APIDumpAnalysis::JsonWriter writer(true); /* in_is_pretty */
        const auto                  status = Analysis::APIDumpAnalysis::QueryCommands::run(in_command,
                                                                                            m_analysis,
                                                                                            m_arguments,
                                                                                           &writer,
                                                                                           &m_error);

        if (status == Analysis::APIDumpAnalysis::QueryStatus::OK)
        {
            EXPECT_FALSE(writer.has_failed() );
            EXPECT_TRUE (Analysis::APIDumpAnalysis::JsonReader::parse(writer.get_text(),
                                                      out_value_ptr,
                                                      nullptr) ) << writer.get_text();
        }

        m_text = writer.get_text();

        return status;
    }

    void SetUp() override
    {
        m_analysis = Tests::DataFixtures::get_fr08_slice_analysis();
    }

    Analysis::APIDumpAnalysis::AnalysisResult       m_analysis;
    Analysis::APIDumpAnalysis::QueryArguments m_arguments;
    std::string                               m_error;
    std::string                               m_text;
};

TEST_F(QueryCommandsTest, EveryCommandAnswersWithJson)
{
    m_arguments.frame_index     = 0;
    m_arguments.has_frame_index = true;
    m_arguments.has_pass_id     = true;
    m_arguments.pass_id         = 0;

    for (const auto& command : Analysis::APIDumpAnalysis::QueryCommands::get_command_names() )
    {
        Analysis::APIDumpAnalysis::JsonValue value;

        EXPECT_EQ(ask(command,
                     &value),
                  Analysis::APIDumpAnalysis::QueryStatus::OK) << command << ": " << m_error;
        EXPECT_EQ(value.type,
                  Analysis::APIDumpAnalysis::JsonValue::Type::OBJECT) << command;
    }
}

TEST_F(QueryCommandsTest, TheOutputIsTheSameEveryTime)
{
    m_arguments.has_pass_id = true;

    for (const auto& command : Analysis::APIDumpAnalysis::QueryCommands::get_command_names() )
    {
        Analysis::APIDumpAnalysis::JsonValue value;

        m_arguments.frame_index     = 0;
        m_arguments.has_frame_index = true;

        ask(command,
           &value);

        const std::string first_text = m_text;

        ask(command,
           &value);

        EXPECT_EQ(first_text,
                  m_text) << command;
    }
}

TEST_F(QueryCommandsTest, CompactOutputIsOnOneLine)
{
    Analysis::APIDumpAnalysis::JsonWriter writer(false); /* in_is_pretty */

    ASSERT_EQ(Analysis::APIDumpAnalysis::QueryCommands::run("info",
                                                            m_analysis,
                                                            m_arguments,
                                                           &writer,
                                                           &m_error),
              Analysis::APIDumpAnalysis::QueryStatus::OK);

    EXPECT_EQ(writer.get_text().find('\n'),
              std::string::npos);
}

TEST_F(QueryCommandsTest, FrameAsksForAFrameOrAShape)
{
    Analysis::APIDumpAnalysis::JsonValue value;

    EXPECT_EQ(ask("frame",
                 &value),
              Analysis::APIDumpAnalysis::QueryStatus::BAD_ARGUMENTS);
    EXPECT_FALSE(m_error.empty() );
}

TEST_F(QueryCommandsTest, AFrameThatIsNotThereIsNotFound)
{
    Analysis::APIDumpAnalysis::JsonValue value;

    m_arguments.frame_index     = 10;
    m_arguments.has_frame_index = true;

    EXPECT_EQ(ask("frame",
                 &value),
              Analysis::APIDumpAnalysis::QueryStatus::NOT_FOUND);
    EXPECT_NE(m_error.find("10"),
              std::string::npos);
}

TEST_F(QueryCommandsTest, FrameTellsWhereItsCallsAreAndHowToPrintThem)
{
    Analysis::APIDumpAnalysis::JsonValue value;

    m_arguments.frame_index     = 1;
    m_arguments.has_frame_index = true;

    ASSERT_EQ(ask("frame",
                 &value),
              Analysis::APIDumpAnalysis::QueryStatus::OK);

    const auto& frame = value.at("frame");
    const auto& calls = frame.at("calls");
    const auto& record = m_analysis.model.frame_record_vec.at(1);

    EXPECT_EQ(frame.at("frame_index").number_value,
              1.0);
    EXPECT_EQ(frame.at("shape_id").number_value,
              static_cast<double>(record.shape_id) );
    EXPECT_EQ(calls.at("first_call_index").number_value,
              static_cast<double>(record.first_call_index) );
    EXPECT_EQ(calls.at("n_calls").number_value,
              static_cast<double>(record.last_call_index - record.first_call_index + 1) );
    EXPECT_NE(calls.at("printer_command").string_value.find("--frame 1"),
              std::string::npos);
    EXPECT_NE(calls.at("printer_command").string_value.find("APIDumpPrinter"),
              std::string::npos);
}

TEST_F(QueryCommandsTest, ARepresentativeFrameComesWithItsPassesAndOthersDoNot)
{
    bool has_frame_with_detail    = false;
    bool has_frame_without_detail = false;

    for (size_t n_frame = 0;
                n_frame < m_analysis.model.frame_record_vec.size();
              ++n_frame)
    {
        Analysis::APIDumpAnalysis::JsonValue value;

        m_arguments.frame_index     = n_frame;
        m_arguments.has_frame_index = true;

        ASSERT_EQ(ask("frame",
                     &value),
                  Analysis::APIDumpAnalysis::QueryStatus::OK);

        const auto& frame = value.at("frame");

        if (frame.at("has_detail").bool_value)
        {
            has_frame_with_detail = true;

            EXPECT_EQ(frame.at("passes").item_vec.size(),
                      static_cast<size_t>(frame.at("n_passes").number_value) );
        }
        else
        {
            has_frame_without_detail = true;

            EXPECT_EQ(frame.find_ptr("passes"),
                      nullptr);
        }
    }

    EXPECT_TRUE(has_frame_with_detail);
    EXPECT_TRUE(has_frame_without_detail);
}

TEST_F(QueryCommandsTest, AShapeLeadsToItsRepresentativeFrames)
{
    const auto n_shapes = m_analysis.model.frame_shape_interner.get_n_structures();

    for (uint64_t n_shape = 0;
                  n_shape < n_shapes;
                ++n_shape)
    {
        Analysis::APIDumpAnalysis::JsonValue value;

        m_arguments.has_shape_id = true;
        m_arguments.shape_id     = n_shape;

        m_arguments.which = Analysis::APIDumpAnalysis::RepresentativeChoice::LIGHTEST;

        ASSERT_EQ(ask("frame",
                     &value),
                  Analysis::APIDumpAnalysis::QueryStatus::OK) << m_error;

        const double lightest = value.at("frame").at("n_draws").number_value;

        m_arguments.which = Analysis::APIDumpAnalysis::RepresentativeChoice::HEAVIEST;

        ASSERT_EQ(ask("frame",
                     &value),
                  Analysis::APIDumpAnalysis::QueryStatus::OK);

        const double heaviest = value.at("frame").at("n_draws").number_value;

        EXPECT_LE(lightest,
                  heaviest);
        EXPECT_EQ(value.at("frame").at("shape_id").number_value,
                  static_cast<double>(n_shape) );
        EXPECT_TRUE(value.at("frame").at("has_detail").bool_value);
        EXPECT_GE(value.at("representatives").item_vec.size(),
                  1u);
    }
}

TEST_F(QueryCommandsTest, AShapeThatIsNotThereIsNotFound)
{
    Analysis::APIDumpAnalysis::JsonValue value;

    m_arguments.has_shape_id = true;
    m_arguments.shape_id     = 99;

    EXPECT_EQ(ask("frame",
                 &value),
              Analysis::APIDumpAnalysis::QueryStatus::NOT_FOUND);
}

TEST_F(QueryCommandsTest, InfoSaysWhatTheAnalysisIsOf)
{
    Analysis::APIDumpAnalysis::JsonValue value;

    ASSERT_EQ(ask("info",
                 &value),
              Analysis::APIDumpAnalysis::QueryStatus::OK);

    EXPECT_EQ(value.at("workload").at("filename").string_value,
              m_analysis.workload_reference.filename);
    EXPECT_GT(value.at("workload").at("n_bytes").number_value,
              0.0);
    EXPECT_EQ(value.at("workload").at("header_hash").string_value.substr(0, 2),
              "0x");
    EXPECT_EQ(value.at("versions").at("file_version").number_value,
              1.0);
    EXPECT_EQ(value.at("summary").at("n_calls").number_value,
              16836.0);
    EXPECT_EQ(value.at("summary").at("n_calls_by_api").at("d3d8").number_value,
              16777.0);
    EXPECT_EQ(value.at("summary").at("n_calls_ignored").number_value,
              59.0);
    EXPECT_FALSE(value.at("summary").at("is_truncated").bool_value);
    EXPECT_GT   (value.at("summary").at("duration_seconds").number_value,
                 0.0);
    EXPECT_EQ(value.at("counts").at("frames").number_value,
              10.0);
    EXPECT_EQ(value.at("counts").at("frame_shapes").number_value,
              4.0);
    EXPECT_EQ(value.at("counts").at("pass_structures").number_value,
              13.0);
    EXPECT_EQ(value.at("counts").at("runs").number_value,
              4.0);
}

TEST_F(QueryCommandsTest, PassTellsWhatAPassIs)
{
    bool has_constant_with_series = false;

    for (uint64_t n_pass = 0;
                  n_pass < m_analysis.model.pass_structure_interner.get_n_structures();
                ++n_pass)
    {
        Analysis::APIDumpAnalysis::JsonValue value;

        m_arguments.has_pass_id = true;
        m_arguments.pass_id     = n_pass;

        ASSERT_EQ(ask("pass",
                     &value),
                  Analysis::APIDumpAnalysis::QueryStatus::OK) << m_error;

        const auto& structure = m_analysis.model.pass_structure_interner.get_structure(static_cast<uint32_t>(n_pass) );

        EXPECT_EQ(value.at("pass_id").number_value,
                  static_cast<double>(n_pass) );
        EXPECT_EQ(value.at("key").string_value.size(),
                  18u);
        EXPECT_GE(value.at("usage").at("n_instances").number_value,
                  value.at("usage").at("n_frames").number_value);
        EXPECT_EQ(value.at("draws").at("total").number_value,
                  static_cast<double>(structure.draw_structure_key_vec.size() ));
        EXPECT_EQ(value.at("draws").at("items").item_vec.size(),
                  structure.draw_structure_key_vec.size() );
        EXPECT_EQ(value.at("constants").item_vec.size(),
                  structure.constant_signature_vec.size() );
        EXPECT_EQ(value.at("viewport").at("width").number_value,
                  static_cast<double>(structure.viewport.width) );

        for (const auto& constant : value.at("constants").item_vec)
        {
            const auto& first = constant.at("series").at("first");

            if (first.find_ptr("value_f32") != nullptr)
            {
                has_constant_with_series = true;

                EXPECT_EQ(first.at("value_f32").item_vec.size() * 4 * 2,
                          first.at("value_hex").string_value.size() );
            }
        }
    }

    EXPECT_TRUE(has_constant_with_series);
}

TEST_F(QueryCommandsTest, PassNeedsAnIdThatExists)
{
    Analysis::APIDumpAnalysis::JsonValue value;

    EXPECT_EQ(ask("pass",
                 &value),
              Analysis::APIDumpAnalysis::QueryStatus::BAD_ARGUMENTS);

    m_arguments.has_pass_id = true;
    m_arguments.pass_id     = 13;

    EXPECT_EQ(ask("pass",
                 &value),
              Analysis::APIDumpAnalysis::QueryStatus::NOT_FOUND);
    EXPECT_NE(m_error.find("13"),
              std::string::npos);
}

TEST_F(QueryCommandsTest, ThePassesOfADrawListArePaged)
{
    Analysis::APIDumpAnalysis::JsonValue value;

    m_arguments.has_pass_id = true;
    m_arguments.limit       = 1;
    m_arguments.pass_id     = 0;

    for (uint64_t n_pass = 0;
                  n_pass < m_analysis.model.pass_structure_interner.get_n_structures();
                ++n_pass)
    {
        m_arguments.pass_id = n_pass;

        ASSERT_EQ(ask("pass",
                     &value),
                  Analysis::APIDumpAnalysis::QueryStatus::OK);

        EXPECT_LE(value.at("draws").at("items").item_vec.size(),
                  1u);
    }
}

TEST_F(QueryCommandsTest, PipelinesListsTheShapesAndTheirPasses)
{
    double           n_frames = 0.0;
    Analysis::APIDumpAnalysis::JsonValue value;

    ASSERT_EQ(ask("pipelines",
                 &value),
              Analysis::APIDumpAnalysis::QueryStatus::OK);

    EXPECT_EQ(value.at("total").number_value,
              4.0);
    ASSERT_EQ(value.at("shapes").item_vec.size(),
              4u);

    for (const auto& shape : value.at("shapes").item_vec)
    {
        double n_instances = 0.0;

        n_frames += shape.at("n_frames").number_value;

        EXPECT_GE(shape.at("passes").item_vec.size(),
                  1u);

        for (const auto& pass : shape.at("passes").item_vec)
        {
            n_instances += pass.at("n_instances").number_value;
        }

        EXPECT_EQ(n_instances,
                  static_cast<double>(m_analysis.model.frame_shape_interner.get_structure(static_cast<uint32_t>(shape.at("shape_id").number_value) ).pass_structure_id_vec.size() ));
    }

    EXPECT_EQ(n_frames,
              10.0);
}

TEST_F(QueryCommandsTest, ListsArePaged)
{
    Analysis::APIDumpAnalysis::JsonValue value;

    m_arguments.limit  = 2;
    m_arguments.offset = 1;

    ASSERT_EQ(ask("pipelines",
                 &value),
              Analysis::APIDumpAnalysis::QueryStatus::OK);

    EXPECT_EQ(value.at("total").number_value,
              4.0);
    EXPECT_EQ(value.at("offset").number_value,
              1.0);
    EXPECT_EQ(value.at("limit").number_value,
              2.0);
    ASSERT_EQ(value.at("shapes").item_vec.size(),
              2u);
    EXPECT_EQ(value.at("shapes").item_vec.at(0).at("shape_id").number_value,
              1.0);

    m_arguments.limit  = 0;
    m_arguments.offset = 0;

    ASSERT_EQ(ask("pipelines",
                 &value),
              Analysis::APIDumpAnalysis::QueryStatus::OK);

    EXPECT_EQ(value.at("shapes").item_vec.size(),
              4u);

    m_arguments.limit  = 100;
    m_arguments.offset = 100;

    ASSERT_EQ(ask("pipelines",
                 &value),
              Analysis::APIDumpAnalysis::QueryStatus::OK);

    EXPECT_EQ(value.at("shapes").item_vec.size(),
              0u);
}

TEST_F(QueryCommandsTest, TimelineListsTheRunsInOrderOfTime)
{
    double           n_frames = 0.0;
    Analysis::APIDumpAnalysis::JsonValue value;

    ASSERT_EQ(ask("timeline",
                 &value),
              Analysis::APIDumpAnalysis::QueryStatus::OK);

    const auto& run_vec = value.at("runs").at("items").item_vec;

    EXPECT_EQ(value.at("runs").at("total").number_value,
              static_cast<double>(run_vec.size() ));

    for (size_t n_run = 0;
                n_run < run_vec.size();
              ++n_run)
    {
        n_frames += run_vec.at(n_run).at("n_frames").number_value;

        EXPECT_EQ(run_vec.at(n_run).at("index").number_value,
                  static_cast<double>(n_run) );

        if (n_run > 0)
        {
            EXPECT_GE(run_vec.at(n_run).at("begin_seconds").number_value,
                      run_vec.at(n_run - 1).at("begin_seconds").number_value);
            EXPECT_EQ(run_vec.at(n_run).at("first_frame_index").number_value,
                      run_vec.at(n_run - 1).at("first_frame_index").number_value +
                      run_vec.at(n_run - 1).at("n_frames").number_value);
        }
    }

    EXPECT_EQ(n_frames,
              10.0);
    EXPECT_EQ(value.at("setup_frames").at("total").number_value,
              static_cast<double>(value.at("setup_frames").at("items").item_vec.size() ));
}

TEST_F(QueryCommandsTest, AnUnknownCommandIsBadArguments)
{
    Analysis::APIDumpAnalysis::JsonValue value;

    EXPECT_EQ(ask("no_such_command",
                 &value),
              Analysis::APIDumpAnalysis::QueryStatus::BAD_ARGUMENTS);
    EXPECT_NE(m_error.find("no_such_command"),
              std::string::npos);
}

TEST_F(QueryCommandsTest, AnAnalysisWithNothingInItCanBeQueried)
{
    Analysis::APIDumpAnalysis::JsonValue value;

    m_analysis = Analysis::APIDumpAnalysis::AnalysisResult();

    EXPECT_EQ(ask("info",
                 &value),
              Analysis::APIDumpAnalysis::QueryStatus::OK);
    EXPECT_EQ(ask("timeline",
                 &value),
              Analysis::APIDumpAnalysis::QueryStatus::OK);
    EXPECT_EQ(ask("pipelines",
                 &value),
              Analysis::APIDumpAnalysis::QueryStatus::OK);

    m_arguments.has_pass_id = true;

    EXPECT_EQ(ask("pass",
                 &value),
              Analysis::APIDumpAnalysis::QueryStatus::NOT_FOUND);
}

TEST_F(QueryCommandsTest, EveryShapeHasAKeyThatIsTheSameWherePassesAreTheSame)
{
    std::set<std::string>                key_set;
    Analysis::APIDumpAnalysis::JsonValue pipelines;

    ASSERT_EQ(ask("pipelines",
                 &pipelines),
              Analysis::APIDumpAnalysis::QueryStatus::OK);

    for (const auto& shape : pipelines.at("shapes").item_vec)
    {
        Analysis::APIDumpAnalysis::JsonValue frame;
        const auto                           shape_id = static_cast<uint32_t>(shape.at("shape_id").number_value);

        EXPECT_EQ(shape.at("shape_key").string_value.size(),
                  18u);

        /* The key is that of the shape's pass keys. */
        std::vector<uint64_t> pass_key_vec;

        for (const auto& pass_id : m_analysis.model.frame_shape_interner.get_structure(shape_id).pass_structure_id_vec)
        {
            pass_key_vec.push_back(m_analysis.model.pass_structure_interner.get_structure(pass_id).key);
        }

        char expected_i8vec32[32];

        ::snprintf(expected_i8vec32,
                   sizeof(expected_i8vec32),
                   "0x%016llx",
                   static_cast<unsigned long long>(Analysis::APIDumpAnalysis::KeyBuilder::compute_key_set_key(pass_key_vec) ));

        EXPECT_EQ(shape.at("shape_key").string_value,
                  expected_i8vec32);

        key_set.insert(shape.at("shape_key").string_value);

        /* A frame of the shape reports the same key. */
        m_arguments.has_shape_id = true;
        m_arguments.shape_id     = shape_id;

        ASSERT_EQ(ask("frame",
                     &frame),
                  Analysis::APIDumpAnalysis::QueryStatus::OK);
        EXPECT_EQ(frame.at("frame").at("shape_key").string_value,
                  shape.at("shape_key").string_value);
    }

    EXPECT_EQ(key_set.size(),
              pipelines.at("shapes").item_vec.size() );
}

TEST_F(QueryCommandsTest, OverlaysAreThePassesWhoseCameraNeverChanges)
{
    Analysis::APIDumpAnalysis::JsonValue value;
    std::set<std::string>                expected_key_set;

    ASSERT_EQ(ask("overlays",
                 &value),
              Analysis::APIDumpAnalysis::QueryStatus::OK) << m_error;

    for (uint32_t n_pass = 0;
                  n_pass < m_analysis.model.pass_structure_interner.get_n_structures();
                ++n_pass)
    {
        const auto& structure     = m_analysis.model.pass_structure_interner.get_structure(n_pass);
        bool        has_camera    = false;
        bool        is_camera_set = false;

        for (size_t n_constant = 0;
                    n_constant < structure.constant_signature_vec.size();
                  ++n_constant)
        {
            const auto series_iter = m_analysis.model.constant_series_map.find(Analysis::APIDumpAnalysis::ConstantSeriesKey(n_pass,
                                                                                                                            static_cast<uint32_t>(n_constant) ));

            if (structure.constant_signature_vec.at(n_constant).semantic == Analysis::APIDumpAnalysis::ConstantSemantic::VIEW_MATRIX &&
                series_iter != m_analysis.model.constant_series_map.end() )
            {
                has_camera    = true;
                is_camera_set = is_camera_set || (series_iter->second.get_sample_vec().size() > 1);
            }
        }

        if (has_camera &&
            is_camera_set == false)
        {
            char key_i8vec32[32];

            ::snprintf(key_i8vec32,
                       sizeof(key_i8vec32),
                       "0x%016llx",
                       static_cast<unsigned long long>(structure.key) );

            expected_key_set.insert(key_i8vec32);
        }
    }

    ASSERT_EQ(value.at("candidates").item_vec.size(),
              expected_key_set.size() );
    ASSERT_EQ(value.at("unaffected").at("pass_keys").item_vec.size(),
              expected_key_set.size() );

    for (const auto& key : value.at("unaffected").at("pass_keys").item_vec)
    {
        EXPECT_EQ(expected_key_set.count(key.string_value),
                  1u);
    }

    EXPECT_FALSE(expected_key_set.empty() );

    /* The draws are those of the candidate passes, one for each signature, and each says which pass it is of. */
    {
        size_t n_expected_draws = 0;

        for (const auto& candidate : value.at("candidates").item_vec)
        {
            const auto& structure = m_analysis.model.pass_structure_interner.get_structure(static_cast<uint32_t>(candidate.at("pass_id").number_value) );

            n_expected_draws += structure.draw_signature_vec.size();
        }

        ASSERT_EQ(value.at("draws").item_vec.size(),
                  n_expected_draws);

        for (const auto& draw : value.at("draws").item_vec)
        {
            EXPECT_EQ(expected_key_set.count(draw.at("pass_key").string_value),
                      1u);
            EXPECT_FALSE(draw.at("signature_key").string_value.empty() );
            EXPECT_FALSE(draw.at("structure_key").string_value.empty() );
            EXPECT_GE(draw.at("n_frames").number_value,
                      1.0);
            EXPECT_EQ(draw.at("reasons").item_vec.at(0).string_value,
                      "static_camera");

            /* A pass that was played at all has a first frame, which is a frame that has it. */
            ASSERT_NE(draw.find_ptr("first_frame_index"),
                      nullptr);
        }
    }

    /* A camera which moves takes the pass out of the list. */
    for (auto& series_pair : m_analysis.model.constant_series_map)
    {
        const auto& structure = m_analysis.model.pass_structure_interner.get_structure(series_pair.first.first);

        if (structure.constant_signature_vec.at(series_pair.first.second).semantic == Analysis::APIDumpAnalysis::ConstantSemantic::VIEW_MATRIX)
        {
            const auto sample_vec = series_pair.second.get_sample_vec();

            series_pair.second.add(sample_vec.back().timestamp_qpc + 1,
                                   std::vector<uint8_t>(sample_vec.back().value_u8_vec.size(),
                                                        0xFF) );

            break;
        }
    }

    Analysis::APIDumpAnalysis::JsonValue moved_value;

    ASSERT_EQ(ask("overlays",
                 &moved_value),
              Analysis::APIDumpAnalysis::QueryStatus::OK) << m_error;
    EXPECT_EQ(moved_value.at("candidates").item_vec.size() + 1,
              expected_key_set.size() );
}

TEST_F(QueryCommandsTest, AStaticCameraPassWhoseVerticesAreAtManyDepthsIsA3DSceneAndIsLeftOutOfTheUnaffectedList)
{
    const auto pass_id_vec = Analysis::APIDumpAnalysis::QueryCommands::get_static_camera_pass_id_vec(m_analysis);

    ASSERT_FALSE(pass_id_vec.empty() );

    const uint32_t pass_id  = pass_id_vec.front();
    const auto     pass_key = m_analysis.model.pass_structure_interner.get_structure(pass_id).key;
    char           key_i8vec32[32];

    ::snprintf(key_i8vec32,
               sizeof(key_i8vec32),
               "0x%016llx",
               static_cast<unsigned long long>(pass_key) );

    /* What the analysis knows of the depth of the pass is replaced by what the test says. */
    for (auto iterator  = m_analysis.model.surface_depth_histogram_map.begin();
              iterator != m_analysis.model.surface_depth_histogram_map.end();)
    {
        iterator = (iterator->first.second == pass_id) ? m_analysis.model.surface_depth_histogram_map.erase(iterator)
                                                       : std::next(iterator);
    }

    for (auto iterator  = m_analysis.model.depth_histogram_map.begin();
              iterator != m_analysis.model.depth_histogram_map.end();)
    {
        iterator = (iterator->first.second == pass_id) ? m_analysis.model.depth_histogram_map.erase(iterator)
                                                       : std::next(iterator);
    }

    const auto is_listed_unaffected_func = [&](const Analysis::APIDumpAnalysis::JsonValue& in_value)
    {
        bool result = false;

        for (const auto& key : in_value.at("unaffected").at("pass_keys").item_vec)
        {
            result = (result || key.string_value == key_i8vec32);
        }

        return result;
    };

    Analysis::APIDumpAnalysis::DepthHistogram flat_histogram;
    Analysis::APIDumpAnalysis::JsonValue      flat_value;

    flat_histogram.add(5.0f,
                       1000);

    m_analysis.model.surface_depth_histogram_map[Analysis::APIDumpAnalysis::DepthHistogramKey(0,
                                                                                              pass_id)] = flat_histogram;

    ASSERT_EQ(ask("overlays",
                 &flat_value),
              Analysis::APIDumpAnalysis::QueryStatus::OK) << m_error;
    EXPECT_TRUE(Analysis::APIDumpAnalysis::QueryCommands::get_pass_depth_spread(m_analysis,
                                                                                 pass_id).is_flat);
    EXPECT_TRUE(is_listed_unaffected_func(flat_value) );

    /* The same pass with vertices from 5 to 500 is a scene. */
    Analysis::APIDumpAnalysis::DepthHistogram deep_histogram;
    Analysis::APIDumpAnalysis::JsonValue      deep_value;

    deep_histogram.add(5.0f,
                       1000);
    deep_histogram.add(50.0f,
                       1000);
    deep_histogram.add(500.0f,
                       1000);

    m_analysis.model.surface_depth_histogram_map[Analysis::APIDumpAnalysis::DepthHistogramKey(0,
                                                                                              pass_id)] = deep_histogram;

    ASSERT_EQ(ask("overlays",
                 &deep_value),
              Analysis::APIDumpAnalysis::QueryStatus::OK) << m_error;

    const auto spread = Analysis::APIDumpAnalysis::QueryCommands::get_pass_depth_spread(m_analysis,
                                                                                         pass_id);

    EXPECT_TRUE (spread.has_depth);
    EXPECT_FALSE(spread.is_flat);
    EXPECT_GT   (spread.far_to_near_ratio,
                 2.0f);
    EXPECT_FALSE(is_listed_unaffected_func(deep_value) );

    /* It is still a candidate, with the reason, so that a person can see why it is not in the list. */
    bool has_deep_reason = false;

    for (const auto& draw : deep_value.at("draws").item_vec)
    {
        if (draw.at("pass_key").string_value == key_i8vec32)
        {
            for (const auto& reason : draw.at("reasons").item_vec)
            {
                has_deep_reason = (has_deep_reason || reason.string_value == "deep_depth");
            }

            EXPECT_FALSE(draw.at("depth").at("is_flat").bool_value);
        }
    }

    EXPECT_TRUE(has_deep_reason);

    /* A pass of which nothing is known of the depth stays in the list, as it did. */
    Analysis::APIDumpAnalysis::JsonValue unknown_value;

    m_analysis.model.surface_depth_histogram_map.erase(Analysis::APIDumpAnalysis::DepthHistogramKey(0,
                                                                                                    pass_id) );

    ASSERT_EQ(ask("overlays",
                 &unknown_value),
              Analysis::APIDumpAnalysis::QueryStatus::OK) << m_error;
    EXPECT_FALSE(Analysis::APIDumpAnalysis::QueryCommands::get_pass_depth_spread(m_analysis,
                                                                                  pass_id).has_depth);
    EXPECT_TRUE(is_listed_unaffected_func(unknown_value) );
}

TEST_F(QueryCommandsTest, ResourcesTellWhatTheFramesDidWithEachOne)
{
    double                               n_resources_in_classes = 0.0;
    Analysis::APIDumpAnalysis::JsonValue value;

    ASSERT_EQ(ask("resources",
                 &value),
              Analysis::APIDumpAnalysis::QueryStatus::OK);

    const auto& item_vec = value.at("resources").at("items").item_vec;

    EXPECT_EQ(value.at("resources").at("total").number_value,
              static_cast<double>(m_analysis.model.resource_vec.size() ));
    EXPECT_EQ(item_vec.size(),
              m_analysis.model.resource_vec.size() );

    for (const auto& count_pair : value.at("counts").item_vec)
    {
        n_resources_in_classes += count_pair.number_value;
    }

    EXPECT_EQ(n_resources_in_classes,
              static_cast<double>(m_analysis.model.resource_vec.size() ));

    for (size_t n_item = 0;
                n_item < item_vec.size();
              ++n_item)
    {
        const auto& access = m_analysis.model.resource_access_vec.at(n_item);

        EXPECT_EQ(item_vec.at(n_item).at("resource_id").number_value,
                  static_cast<double>(n_item + 1) );
        EXPECT_EQ(item_vec.at(n_item).at("n_frames_read").number_value,
                  static_cast<double>(access.n_frames_read) );
        EXPECT_EQ(item_vec.at(n_item).at("n_frames_written").number_value,
                  static_cast<double>(access.n_frames_written) );
        EXPECT_FALSE(item_vec.at(n_item).at("access").string_value.empty() );

        /* A resource that was never written has no first write. */
        EXPECT_EQ(item_vec.at(n_item).at("first_write_frame_index").type == Analysis::APIDumpAnalysis::JsonValue::Type::NULL_VALUE,
                  access.n_frames_written == 0);
    }
}

TEST_F(QueryCommandsTest, ResourcesCanBeListedByTheirClass)
{
    const Analysis::APIDumpAnalysis::ResourceAccessClass class_array[] =
    {
        Analysis::APIDumpAnalysis::ResourceAccessClass::FRAME_LOCAL,
        Analysis::APIDumpAnalysis::ResourceAccessClass::LOOP_CARRIED,
        Analysis::APIDumpAnalysis::ResourceAccessClass::STATIC,
        Analysis::APIDumpAnalysis::ResourceAccessClass::UNREAD,
        Analysis::APIDumpAnalysis::ResourceAccessClass::UNUSED
    };

    double n_listed = 0.0;

    for (const auto& access_class : class_array)
    {
        Analysis::APIDumpAnalysis::JsonValue value;

        m_arguments.access_class     = access_class;
        m_arguments.has_access_class = true;

        ASSERT_EQ(ask("resources",
                     &value),
                  Analysis::APIDumpAnalysis::QueryStatus::OK);

        n_listed += value.at("resources").at("total").number_value;

        for (const auto& item : value.at("resources").at("items").item_vec)
        {
            EXPECT_EQ(Analysis::APIDumpAnalysis::ResourceAccessTracker::classify(m_analysis.model.resource_access_vec.at(static_cast<size_t>(item.at("resource_id").number_value) - 1) ),
                      access_class);
        }
    }

    /* Every resource has exactly one class. */
    EXPECT_EQ(n_listed,
              static_cast<double>(m_analysis.model.resource_vec.size() ));
}

TEST_F(QueryCommandsTest, ClassNamesAreReadBack)
{
    Analysis::APIDumpAnalysis::ResourceAccessClass access_class = Analysis::APIDumpAnalysis::ResourceAccessClass::UNUSED;

    EXPECT_TRUE(Analysis::APIDumpAnalysis::QueryCommands::parse_resource_access_class("loop_carried",
                                                                                     &access_class) );
    EXPECT_EQ  (access_class,
                Analysis::APIDumpAnalysis::ResourceAccessClass::LOOP_CARRIED);
    EXPECT_FALSE(Analysis::APIDumpAnalysis::QueryCommands::parse_resource_access_class("loop carried",
                                                                                      &access_class) );
    EXPECT_FALSE(Analysis::APIDumpAnalysis::QueryCommands::parse_resource_access_class("",
                                                                                      &access_class) );
}

TEST_F(QueryCommandsTest, InfoCountsTheResourcesOfEachClass)
{
    double                               n_resources = 0.0;
    Analysis::APIDumpAnalysis::JsonValue value;

    ASSERT_EQ(ask("info",
                 &value),
              Analysis::APIDumpAnalysis::QueryStatus::OK);

    for (const auto& count : value.at("counts").at("resources_by_access").item_vec)
    {
        n_resources += count.number_value;
    }

    EXPECT_EQ(n_resources,
              static_cast<double>(m_analysis.model.resource_vec.size() ));
    EXPECT_EQ(value.at("counts").at("resources_by_access").key_vec.size(),
              5u);
}
