/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Checks of the analysis of a workload from a file to a model: what is read, what is skipped, and what is said about it.
 */
#include <gtest/gtest.h>
#include "analysis/api_dump_analysis/frontends/analysis_session.h"
#include "analysis/api_dump_analysis/key_builder.h"
#include "Capture/APIDumpLoader/WorkloadReader.h"
#include "d3d8_call_builder.h"
#include "data_fixtures.h"
#include "workload_file_builder.h"
#include <stdio.h>

TEST(AnalysisSession, TheSliceOfFr08IsAnalyzedAndDescribed)
{
    Analysis::APIDumpAnalysis::Frontends::AnalysisSession session;
    std::string                                           error;

    ASSERT_TRUE(session.run(Tests::DataFixtures::get_fr08_slice_filename().c_str(),
                                nullptr,
                               &error) ) << error;

    const auto  diagnostics = session.get_summary();
    const auto& model       = session.get_model();

    EXPECT_EQ(model.frame_record_vec.size(),
              10u);
    EXPECT_EQ(model.pass_structure_interner.get_n_structures(),
              13u);

    EXPECT_EQ(diagnostics.n_calls,
              16836u);
    EXPECT_EQ(diagnostics.n_calls_by_group_map.at(Capture::APIDumpLoader::APIGroup::D3D8),
              16777u);
    EXPECT_EQ(diagnostics.n_calls_by_group_map.at(Capture::APIDumpLoader::APIGroup::USER32),
              59u);
    EXPECT_EQ(diagnostics.n_calls_ignored,
              59u);
    EXPECT_EQ(diagnostics.n_segments,
              1u);
    EXPECT_EQ(diagnostics.n_trailing_calls,
              0u);
    EXPECT_EQ(diagnostics.n_unfollowed_calls,
              0u);
    EXPECT_EQ(diagnostics.n_unsupported_calls,
              0u);
    EXPECT_EQ(diagnostics.pointer_n_bytes,
              4u);
    EXPECT_EQ(diagnostics.qpc_frequency,
              10000000u);
    EXPECT_FALSE(diagnostics.is_truncated);
    EXPECT_LT   (diagnostics.first_timestamp_qpc,
                 diagnostics.last_timestamp_qpc);
}

TEST(AnalysisSession, ProgressIsReportedWithWhatIsFoundSoFar)
{
    Analysis::APIDumpAnalysis::Frontends::AnalysisSession session;
    std::vector<Analysis::APIDumpAnalysis::Frontends::SessionProgress> progress_vec;

    const auto progress_func = [&](const Analysis::APIDumpAnalysis::Frontends::SessionProgress& in_progress)
    {
        progress_vec.push_back(in_progress);
    };

    ASSERT_TRUE(session.run(Tests::DataFixtures::get_fr08_slice_filename().c_str(),
                                progress_func,
                                nullptr) );

    ASSERT_GE(progress_vec.size(),
              1u);

    EXPECT_EQ(progress_vec.back().n_calls,
              16836u);
    EXPECT_EQ(progress_vec.back().n_frames,
              10u);
    EXPECT_EQ(progress_vec.back().n_frame_shapes,
              4u);
    EXPECT_EQ(progress_vec.back().n_segments,
              1u);
    EXPECT_GT(progress_vec.back().demo_seconds,
              0.0);
}

TEST(AnalysisSession, AFileThatCannotBeReadIsReported)
{
    Analysis::APIDumpAnalysis::Frontends::AnalysisSession session;
    std::string                                           error;

    EXPECT_FALSE(session.run("this_file_does_not_exist.workload",
                             nullptr,
                            &error) );
    EXPECT_FALSE(error.empty() );
}

TEST(AnalysisSession, ACutOffWorkloadIsAnalyzedAsFarAsItGoes)
{
    Analysis::APIDumpAnalysis::Frontends::AnalysisSession session;
    std::vector<uint8_t>                                  file_u8_vec;
    FILE*                                                 file_handle = ::fopen(Tests::DataFixtures::get_fr08_slice_filename().c_str(),
                                                                          "rb");

    ASSERT_NE(file_handle,
              nullptr);

    ::fseek(file_handle,
            0,
            SEEK_END);

    file_u8_vec.resize(static_cast<size_t>(::ftell(file_handle) ));

    ::fseek(file_handle,
            0,
            SEEK_SET);
    ::fread(file_u8_vec.data(),
            file_u8_vec.size(),
            1,
            file_handle);
    ::fclose(file_handle);

    /* A tenth of the workload is there. */
    file_u8_vec.resize(file_u8_vec.size() / 10);
    Tests::WorkloadFileBuilder::write_file(file_u8_vec);

    ASSERT_TRUE(session.run(Tests::WorkloadFileBuilder::get_filename_ptr(),
                            nullptr,
                            nullptr) );

    EXPECT_TRUE(session.get_summary().is_truncated);
    EXPECT_GT  (session.get_summary().n_calls,
                0u);
    EXPECT_LT  (session.get_summary().n_calls,
                16836u);

    ::remove(Tests::WorkloadFileBuilder::get_filename_ptr() );
}

TEST(AnalysisSession, TheCallsOfAnAPIWithoutAFrontEndAreCountedAndSkipped)
{
    Analysis::APIDumpAnalysis::Frontends::AnalysisSession session;
    std::vector<uint8_t>                                  file_u8_vec;

    Tests::WorkloadFileBuilder::build_frames(&file_u8_vec,
                                             2,  /* in_n_frames          */
                                             3); /* in_n_calls_per_frame */
    Tests::WorkloadFileBuilder::write_file  (file_u8_vec);

    ASSERT_TRUE(session.run(Tests::WorkloadFileBuilder::get_filename_ptr(),
                            nullptr,
                            nullptr) );

    const auto diagnostics = session.get_summary();

    EXPECT_EQ(diagnostics.n_calls,
              8u);
    EXPECT_EQ(diagnostics.n_calls_ignored,
              8u);
    EXPECT_EQ(diagnostics.n_calls_by_group_map.at(Capture::APIDumpLoader::APIGroup::GDI32),
              8u);
    EXPECT_EQ(session.get_model().frame_record_vec.size(),
              0u);

    ::remove(Tests::WorkloadFileBuilder::get_filename_ptr() );
}

TEST(AnalysisSession, CallsAfterTheLastPresentAreNotInAnyFrame)
{
    Analysis::APIDumpAnalysis::Frontends::AnalysisSession session;
    Tests::D3D8CallBuilder                                builder;

    builder.create_device();
    builder.present();
    builder.begin_scene();
    builder.end_scene();

    session.process_segment(builder.get_workload() );

    EXPECT_EQ(session.get_model().frame_record_vec.size(),
              1u);
    EXPECT_EQ(session.get_summary().n_trailing_calls,
              2u);
}

TEST(AnalysisSession, SegmentsAreAnalyzedOneAfterTheOther)
{
    Analysis::APIDumpAnalysis::Frontends::AnalysisSession session;
    Tests::D3D8CallBuilder                                first;
    Tests::D3D8CallBuilder                                second;

    first.create_device();
    first.present();

    session.process_segment(first.get_workload() );

    EXPECT_EQ(session.get_model().frame_record_vec.size(),
              1u);
    EXPECT_EQ(session.get_summary().n_segments,
              1u);

    /* The second segment is made of calls that refer to the device made in the first one: a front end that carries on
     * does not need it again.
     */
    second.present();

    session.process_segment(second.get_workload() );

    EXPECT_EQ(session.get_model().frame_record_vec.size(),
              2u);
    EXPECT_EQ(session.get_summary().n_segments,
              2u);
    EXPECT_EQ(session.get_summary().n_calls,
              4u); /* Direct3DCreate8, CreateDevice and a present; and a present */
}

TEST(AnalysisSession, AnalysisStopsAtTheFrameLimit)
{
    Analysis::APIDumpAnalysis::Frontends::AnalysisSession session;

    session.set_max_frames(3);

    ASSERT_TRUE(session.run(Tests::DataFixtures::get_fr08_slice_filename().c_str(),
                                nullptr,
                                nullptr) );

    EXPECT_TRUE(session.has_reached_frame_limit() );
    EXPECT_EQ  (session.get_model().frame_record_vec.size(),
                3u);
}

TEST(AnalysisSession, TheWorkloadIsReferredToByItsNameAndSize)
{
    Analysis::APIDumpAnalysis::Frontends::AnalysisSession session;
    const std::string                                     filename = Tests::DataFixtures::get_fr08_slice_filename();

    ASSERT_TRUE(session.run(filename.c_str(),
                            nullptr,
                            nullptr) );

    EXPECT_EQ(session.get_workload_reference().filename,
              filename);
    EXPECT_GT(session.get_workload_reference().n_bytes,
              0u);
    EXPECT_NE(session.get_workload_reference().header_hash,
              0u);
}

/* The shape key of a frame, for a model that keeps its frames. */
static uint64_t get_shape_key_of_frame(const Analysis::APIDumpAnalysis::Model& in_model,
                                       const size_t&                           in_frame_index)
{
    std::vector<uint64_t> pass_key_vec;
    const auto&           shape = in_model.frame_shape_interner.get_structure(in_model.frame_record_vec.at(in_frame_index).shape_id);

    for (const auto& pass_id : shape.pass_structure_id_vec)
    {
        pass_key_vec.push_back(in_model.pass_structure_interner.get_structure(pass_id).key);
    }

    return Analysis::APIDumpAnalysis::KeyBuilder::compute_key_set_key(pass_key_vec);
}

TEST(AnalysisSession, CallsAnalyzedOneByOneAreFramesOfTheSameKindsAsThoseOfAFile)
{
    Analysis::APIDumpAnalysis::Frontends::AnalysisSession  file_session;
    std::vector<Analysis::APIDumpAnalysis::CompletedFrame> live_frame_vec;
    Analysis::APIDumpAnalysis::Frontends::AnalysisSession  live_session(false); /* in_keeps_history */
    Capture::APIDumpLoader::Workload             segment;
    auto                                         reader_ptr = Capture::APIDumpLoader::WorkloadReader::create(Tests::DataFixtures::get_fr08_slice_filename().c_str(),
                                                                                                                 nullptr);

    ASSERT_NE(reader_ptr,
              nullptr);
    ASSERT_TRUE(file_session.run(Tests::DataFixtures::get_fr08_slice_filename().c_str(),
                                     nullptr,
                                     nullptr) );

    while (reader_ptr->read_next_segment(&segment) )
    {
        for (const auto& call : *segment.dumped_api_call_vec_ptr)
        {
            if (live_session.process_call(call,
                                          segment) )
            {
                live_frame_vec.push_back(live_session.get_completed_frame() );
            }
        }
    }

    const auto& file_model = file_session.get_model();

    ASSERT_EQ(live_frame_vec.size(),
              file_model.frame_record_vec.size() );
    ASSERT_GT(live_frame_vec.size(),
              0u);

    for (size_t n_frame = 0;
                n_frame < live_frame_vec.size();
              ++n_frame)
    {
        const auto& frame_record = file_model.frame_record_vec.at(n_frame);
        const auto& live_frame   = live_frame_vec.at(n_frame);

        EXPECT_EQ(live_frame.frame_index,
                  n_frame);
        EXPECT_EQ(live_frame.shape_key,
                  get_shape_key_of_frame(file_model,
                                         n_frame) ) << "frame " << n_frame;
        EXPECT_EQ(live_frame.pass_instance_vec.size(),
                  frame_record.n_passes);
        EXPECT_EQ(live_frame.first_call_index,
                  frame_record.first_call_index);
        EXPECT_EQ(live_frame.last_call_index,
                  frame_record.last_call_index);
    }

    /* The live session keeps no frames. */
    EXPECT_TRUE(live_session.get_model().frame_record_vec.empty() );
    EXPECT_EQ  (live_session.get_model().pass_structure_interner.get_n_structures(),
                file_model.pass_structure_interner.get_n_structures() );
}

TEST(AnalysisSession, TheCallsOfARealFrameBelongToThePassesInOrder)
{
    size_t                                      n_frames_checked = 0;
    auto                                        reader_ptr       = Capture::APIDumpLoader::WorkloadReader::create(Tests::DataFixtures::get_fr08_slice_filename().c_str(),
                                                                                                                      nullptr);
    Capture::APIDumpLoader::Workload                     segment;
    Analysis::APIDumpAnalysis::Frontends::AnalysisSession session(false); /* in_keeps_history */

    ASSERT_NE(reader_ptr,
              nullptr);

    while (reader_ptr->read_next_segment(&segment) )
    {
        for (const auto& call : *segment.dumped_api_call_vec_ptr)
        {
            if (session.process_call(call,
                                     segment) == false)
            {
                continue;
            }

            const auto& frame           = session.get_completed_frame();
            uint32_t    last_pass_index = 0;

            for (uint64_t n_call = frame.first_call_index;
                          n_call <= frame.last_call_index;
                        ++n_call)
            {
                uint32_t pass_index = 0;

                if (Analysis::APIDumpAnalysis::ModelBuilder::find_pass_index(frame,
                                                                             n_call,
                                                                            &pass_index) )
                {
                    /* Passes are in call order. */
                    EXPECT_GE(pass_index,
                              last_pass_index);

                    last_pass_index = pass_index;
                }
                else
                {
                    /* Only calls after the last draw belong to no pass. */
                    EXPECT_GT(n_call,
                              frame.pass_instance_vec.back().last_draw_call_index);
                }
            }

            for (size_t n_pass = 0;
                        n_pass < frame.pass_instance_vec.size();
                      ++n_pass)
            {
                uint32_t pass_index = 0;

                ASSERT_TRUE(Analysis::APIDumpAnalysis::ModelBuilder::find_pass_index(frame,
                                                                                     frame.pass_instance_vec.at(n_pass).last_draw_call_index,
                                                                                    &pass_index) );
                EXPECT_EQ  (pass_index,
                            n_pass);
            }

            ++n_frames_checked;
        }
    }

    EXPECT_EQ(n_frames_checked,
              10u);
}

TEST(AnalysisSession, TheModelCanBeTakenInsteadOfCopiedAndTheSummaryStaysAvailable)
{
    Analysis::APIDumpAnalysis::Frontends::AnalysisSession session;
    std::string                                           error;

    ASSERT_TRUE(session.run(Tests::DataFixtures::get_fr08_slice_filename().c_str(),
                                nullptr,
                               &error) ) << error;

    const auto copy  = session.get_model();
    const auto taken = session.take_model();

    EXPECT_EQ(taken.frame_record_vec.size(),
              copy.frame_record_vec.size() );
    EXPECT_EQ(taken.pass_structure_interner.get_n_structures(),
              copy.pass_structure_interner.get_n_structures() );
    EXPECT_EQ(taken.resource_vec.size(),
              copy.resource_vec.size() );
    EXPECT_EQ(session.get_summary().n_calls,
              16836u);
}

TEST(AnalysisSession, TheDrawsOfEveryFrameMatchTheCallsAndThePassesOfTheFrame)
{
    const auto reader_ptr = Capture::APIDumpLoader::WorkloadReader::create(Tests::DataFixtures::get_fr08_slice_filename().c_str() );

    Capture::APIDumpLoader::Workload                     segment;
    Analysis::APIDumpAnalysis::Frontends::AnalysisSession session(false); /* in_keeps_history */
    uint32_t                                    n_frames_checked = 0;

    ASSERT_NE(reader_ptr,
              nullptr);

    while (reader_ptr->read_next_segment(&segment) )
    {
        for (const auto& call : *segment.dumped_api_call_vec_ptr)
        {
            if (session.process_call(call,
                                     segment) == false)
            {
                continue;
            }

            const auto& frame = session.get_completed_frame();
            uint64_t    n_draws_of_passes = 0;
            uint64_t    previous_call     = 0;

            for (const auto& pass : frame.pass_instance_vec)
            {
                n_draws_of_passes += pass.n_draws;
            }

            ASSERT_EQ(frame.draw_instance_vec.size(),
                      n_draws_of_passes);

            for (const auto& draw : frame.draw_instance_vec)
            {
                ASSERT_LT(draw.pass_index,
                          frame.pass_instance_vec.size() );

                /* In the order of the calls, inside the frame, and not after the last draw of the pass. */
                EXPECT_GE(draw.call_index,
                          frame.first_call_index);
                EXPECT_LE(draw.call_index,
                          frame.pass_instance_vec.at(draw.pass_index).last_draw_call_index);
                EXPECT_GT(draw.call_index + 1,
                          previous_call + 1);

                previous_call = draw.call_index;

                EXPECT_NE(session.get_model().draw_structure_map.find(draw.structure_key),
                          session.get_model().draw_structure_map.end() );
            }

            ++n_frames_checked;
        }
    }

    EXPECT_EQ(n_frames_checked,
              10u);
}
