/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Checks of the model builder with events made up by hand: where passes begin and end, and what their keys do and do not
 * depend on.
 */
#include <gtest/gtest.h>
#include "Analysis/APIDumpAnalysis/resource_access_tracker.h"
#include "model_test_helper.h"
#include <algorithm>
#include <set>

TEST(ModelBuilder, AFrameWithoutWorkHasNoPasses)
{
    Tests::ModelTestHelper helper;

    helper.end_frame();

    const auto& model = helper.get_model();

    ASSERT_EQ(model.frame_record_vec.size(),
              1u);
    EXPECT_EQ(model.frame_record_vec.at(0).n_passes,
              0u);
    EXPECT_EQ(model.frame_record_vec.at(0).n_draws,
              0u);
    EXPECT_TRUE(model.frame_shape_interner.get_structure(model.frame_record_vec.at(0).shape_id).pass_structure_id_vec.empty() );
}

TEST(ModelBuilder, DrawsToOneTargetAreOnePass)
{
    Tests::ModelTestHelper helper;
    const auto             swapchain_image = helper.create_swapchain_image();

    helper.bind(swapchain_image);
    helper.draw(0, 3);
    helper.draw(0, 6);
    helper.draw(0, 9);
    helper.end_frame();

    const auto& model = helper.get_model();

    ASSERT_EQ(model.frame_record_vec.size(),
              1u);
    EXPECT_EQ(model.frame_record_vec.at(0).n_passes,
              1u);
    EXPECT_EQ(model.frame_record_vec.at(0).n_draws,
              3u);

    const auto entry_ptr_vec = model.representative_store.get_entry_ptr_vec(model.frame_record_vec.at(0).shape_id);

    ASSERT_EQ(entry_ptr_vec.size(),
              1u);
    ASSERT_EQ(entry_ptr_vec.at(0)->detail.pass_instance_vec.size(),
              1u);
    EXPECT_EQ(entry_ptr_vec.at(0)->detail.pass_instance_vec.at(0).n_draws,
              3u);
    EXPECT_EQ(entry_ptr_vec.at(0)->detail.pass_instance_vec.at(0).n_vertices,
              18u);
}

TEST(ModelBuilder, ABindWithoutDrawsDoesNotStartAPass)
{
    Tests::ModelTestHelper rebound;
    Tests::ModelTestHelper direct;

    {
        const auto texture         = rebound.create_color_texture();
        const auto swapchain_image = rebound.create_swapchain_image();

        rebound.bind(texture);
        rebound.bind(swapchain_image);
        rebound.draw();
        rebound.end_frame();
    }

    {
        direct.create_color_texture();

        const auto swapchain_image = direct.create_swapchain_image();

        direct.bind(swapchain_image);
        direct.draw();
        direct.end_frame();
    }

    EXPECT_EQ(rebound.get_model().frame_record_vec.at(0).n_passes,
              1u);
    EXPECT_EQ(rebound.get_ordered_pass_keys(0),
              direct.get_ordered_pass_keys(0) );
}

TEST(ModelBuilder, ABindAfterADrawStartsANewPassEvenToTheSameTarget)
{
    Tests::ModelTestHelper helper;
    const auto             swapchain_image = helper.create_swapchain_image();

    helper.bind(swapchain_image);
    helper.draw();
    helper.bind(swapchain_image);
    helper.draw();
    helper.end_frame();

    EXPECT_EQ(helper.get_model().frame_record_vec.at(0).n_passes,
              2u);
}

TEST(ModelBuilder, AClearAfterADrawStartsANewPassButClearsBeforeItDoNot)
{
    Tests::ModelTestHelper helper;
    const auto             depth_buffer    = helper.create_depth_buffer();
    const auto             swapchain_image = helper.create_swapchain_image();

    helper.bind (swapchain_image,
                 depth_buffer);
    helper.draw ();
    helper.clear(false,  /* in_clears_color */
                 true);  /* in_clears_depth */
    helper.draw ();
    helper.end_frame();

    helper.bind (swapchain_image,
                 depth_buffer);
    helper.clear(true,   /* in_clears_color */
                 false); /* in_clears_depth */
    helper.clear(false,  /* in_clears_color */
                 true);  /* in_clears_depth */
    helper.draw ();
    helper.end_frame();

    EXPECT_EQ(helper.get_model().frame_record_vec.at(0).n_passes,
              2u);
    EXPECT_EQ(helper.get_model().frame_record_vec.at(1).n_passes,
              1u);
}

TEST(ModelBuilder, AViewportChangeAfterADrawStartsANewPassButTheSameViewportDoesNot)
{
    Tests::ModelTestHelper helper;
    const auto             swapchain_image = helper.create_swapchain_image();

    helper.bind        (swapchain_image);
    helper.set_viewport(0, 0, 800, 600);
    helper.draw        ();
    helper.set_viewport(0, 0, 800, 600);
    helper.draw        ();
    helper.end_frame   ();

    helper.bind        (swapchain_image);
    helper.set_viewport(0, 0, 800, 600);
    helper.draw        ();
    helper.set_viewport(0, 45, 800, 510);
    helper.draw        ();
    helper.end_frame   ();

    EXPECT_EQ(helper.get_model().frame_record_vec.at(0).n_passes,
              1u);
    EXPECT_EQ(helper.get_model().frame_record_vec.at(1).n_passes,
              2u);
}

TEST(ModelBuilder, ADifferentViewportIsADifferentPass)
{
    Tests::ModelTestHelper helper;
    const auto             swapchain_image = helper.create_swapchain_image();

    helper.bind        (swapchain_image);
    helper.set_viewport(0, 0, 800, 600);
    helper.draw        ();
    helper.end_frame   ();

    helper.bind        (swapchain_image);
    helper.set_viewport(0, 45, 800, 510);
    helper.draw        ();
    helper.end_frame   ();

    EXPECT_NE(helper.get_ordered_pass_keys(0),
              helper.get_ordered_pass_keys(1) );
}

TEST(ModelBuilder, AClearWithoutDrawsIsAPass)
{
    Tests::ModelTestHelper helper;
    const auto             texture = helper.create_color_texture();

    helper.bind     (texture);
    helper.clear    (true,   /* in_clears_color */
                     false); /* in_clears_depth */
    helper.end_frame();

    const auto& model = helper.get_model();

    ASSERT_EQ(model.frame_record_vec.at(0).n_passes,
              1u);
    EXPECT_EQ(model.frame_record_vec.at(0).n_draws,
              0u);

    const auto& structure = model.pass_structure_interner.get_structure(0);

    EXPECT_TRUE(structure.clears_color);
    EXPECT_FALSE(structure.clears_depth);
    EXPECT_TRUE(structure.draw_structure_key_vec.empty() );
}

TEST(ModelBuilder, APassDependsOnThePassThatRenderedWhatItSamples)
{
    const auto run_func = [](const uint32_t& in_first_pass_state)
    {
        Tests::ModelTestHelper helper;
        const auto             swapchain_image = helper.create_swapchain_image();
        const auto             texture         = helper.create_color_texture();

        helper.bind         (texture);
        helper.clear        (true,   /* in_clears_color */
                             false); /* in_clears_depth */
        helper.draw         (in_first_pass_state);
        helper.bind         (swapchain_image);
        helper.draw_textured(texture);
        helper.end_frame    ();

        return helper.get_ordered_pass_keys(0);
    };

    const auto first  = run_func(1);
    const auto second = run_func(2);
    const auto again  = run_func(1);

    ASSERT_EQ(first.size(),
              2u);

    EXPECT_EQ(first,
              again);
    EXPECT_NE(first.at(0),
              second.at(0) );
    EXPECT_NE(first.at(1),
              second.at(1) );
}

TEST(ModelBuilder, APassDoesNotDependOnAPassThatRenderedSomethingElse)
{
    const auto run_func = [](const uint32_t& in_first_pass_state)
    {
        Tests::ModelTestHelper helper;
        const auto             swapchain_image = helper.create_swapchain_image();
        const auto             static_texture  = helper.create_static_texture();
        const auto             texture         = helper.create_color_texture();

        helper.bind         (texture);
        helper.draw         (in_first_pass_state);
        helper.bind         (swapchain_image);
        helper.draw_textured(static_texture);
        helper.end_frame    ();

        return helper.get_ordered_pass_keys(0);
    };

    const auto first  = run_func(1);
    const auto second = run_func(2);

    ASSERT_EQ(first.size(),
              2u);

    EXPECT_NE(first.at(0),
              second.at(0) );
    EXPECT_EQ(first.at(1),
              second.at(1) );
}

TEST(ModelBuilder, AClearEndsTheDependencyOnWhatWasThere)
{
    Tests::ModelTestHelper after_a_pass;
    Tests::ModelTestHelper alone;

    {
        const auto swapchain_image = after_a_pass.create_swapchain_image();

        after_a_pass.bind     (swapchain_image);
        after_a_pass.draw     (1);
        after_a_pass.bind     (swapchain_image);
        after_a_pass.clear    (true,   /* in_clears_color */
                               false); /* in_clears_depth */
        after_a_pass.draw     (2);
        after_a_pass.end_frame();
    }

    {
        const auto swapchain_image = alone.create_swapchain_image();

        alone.bind     (swapchain_image);
        alone.clear    (true,   /* in_clears_color */
                        false); /* in_clears_depth */
        alone.draw     (2);
        alone.end_frame();
    }

    ASSERT_EQ(after_a_pass.get_ordered_pass_keys(0).size(),
              2u);
    EXPECT_EQ(after_a_pass.get_ordered_pass_keys(0).at(1),
              alone.get_ordered_pass_keys(0).at(0) );
}

TEST(ModelBuilder, PingPongPassesHaveTheSameKeysWhicheverTextureStartsIt)
{
    const auto run_func = [](const bool& in_swap)
    {
        Tests::ModelTestHelper helper;
        const auto             swapchain_image = helper.create_swapchain_image();
        const auto             texture_a       = helper.create_color_texture();
        const auto             texture_b       = helper.create_color_texture();
        const auto             first           = (in_swap) ? texture_b : texture_a;
        const auto             second          = (in_swap) ? texture_a : texture_b;

        helper.bind         (first);
        helper.clear        (true,   /* in_clears_color */
                             false); /* in_clears_depth */
        helper.draw         (1);
        helper.bind         (second);
        helper.draw_textured(first);
        helper.bind         (first);
        helper.draw_textured(second);
        helper.bind         (swapchain_image);
        helper.draw_textured(first);
        helper.end_frame    ();

        return helper.get_ordered_pass_keys(0);
    };

    const auto plain   = run_func(false);
    const auto swapped = run_func(true);

    ASSERT_EQ(plain.size(),
              4u);

    EXPECT_EQ(plain,
              swapped);

    /* The passes of the chain are not the same pass, though. */
    EXPECT_NE(plain.at(1),
              plain.at(2) );
}

TEST(ModelBuilder, TheOrderOfCreationCallsDoesNotChangeTheKeys)
{
    Tests::ModelTestHelper in_order;
    Tests::ModelTestHelper shuffled;

    {
        const auto swapchain_image = in_order.create_swapchain_image();
        const auto texture         = in_order.create_color_texture();
        const auto static_texture  = in_order.create_static_texture();

        in_order.bind         (texture);
        in_order.draw         (1);
        in_order.bind         (swapchain_image);
        in_order.draw_textured(texture);
        in_order.draw_textured(static_texture);
        in_order.end_frame    ();
    }

    {
        /* Another thread made a buffer in between, and the textures were made in another order. */
        const auto static_texture  = shuffled.create_static_texture();
        shuffled.create_vertex_buffer();
        const auto texture         = shuffled.create_color_texture();
        const auto swapchain_image = shuffled.create_swapchain_image();

        shuffled.bind         (texture);
        shuffled.draw         (1);
        shuffled.bind         (swapchain_image);
        shuffled.draw_textured(texture);
        shuffled.draw_textured(static_texture);
        shuffled.end_frame    ();
    }

    EXPECT_EQ(in_order.get_ordered_pass_keys(0),
              shuffled.get_ordered_pass_keys(0) );
    EXPECT_EQ(in_order.get_shape_key(0),
              shuffled.get_shape_key(0) );
}

TEST(ModelBuilder, PassesThatDoNotDependOnEachOtherMayComeInAnyOrder)
{
    const auto run_func = [](const bool& in_reverse)
    {
        Tests::ModelTestHelper helper;
        const auto             texture_a = helper.create_color_texture();
        const auto             texture_b = helper.create_static_texture();
        const auto             swapchain_image = helper.create_swapchain_image();

        for (uint32_t n_pass = 0;
                      n_pass < 2;
                    ++n_pass)
        {
            if ((n_pass == 0) != in_reverse)
            {
                helper.bind(texture_a);
                helper.draw(1);
            }
            else
            {
                helper.bind(swapchain_image);
                helper.draw_textured(texture_b);
            }
        }

        helper.end_frame();

        return helper;
    };

    const auto forward = run_func(false);
    const auto reverse = run_func(true);

    EXPECT_EQ(forward.get_shape_key(0),
              reverse.get_shape_key(0) );
    EXPECT_NE(forward.get_ordered_pass_keys(0),
              reverse.get_ordered_pass_keys(0) );
}

TEST(ModelBuilder, DrawCountsDoNotChangeTheShapeButWhatIsDrawnDoes)
{
    Tests::ModelTestHelper helper;
    const auto             swapchain_image = helper.create_swapchain_image();

    helper.bind(swapchain_image);

    for (uint32_t n_draw = 0;
                  n_draw < 3;
                ++n_draw)
    {
        helper.draw(1);
    }

    helper.end_frame();

    helper.bind(swapchain_image);

    for (uint32_t n_draw = 0;
                  n_draw < 10;
                ++n_draw)
    {
        helper.draw(1);
    }

    helper.end_frame();

    helper.bind(swapchain_image);
    helper.draw(1);
    helper.draw(2);
    helper.end_frame();

    const auto& frame_record_vec = helper.get_model().frame_record_vec;

    EXPECT_EQ(frame_record_vec.at(0).shape_id,
              frame_record_vec.at(1).shape_id);
    EXPECT_NE(frame_record_vec.at(0).shape_id,
              frame_record_vec.at(2).shape_id);
}

TEST(ModelBuilder, TheFirstTheLightestAndTheHeaviestFramesOfAShapeKeepTheirDetail)
{
    const uint32_t         n_draws_per_frame[] = {5, 3, 9, 5, 6};
    Tests::ModelTestHelper helper;
    const auto             swapchain_image     = helper.create_swapchain_image();

    for (const auto& n_draws : n_draws_per_frame)
    {
        helper.bind(swapchain_image);

        for (uint32_t n_draw = 0;
                      n_draw < n_draws;
                    ++n_draw)
        {
            helper.draw(1);
        }

        helper.end_frame();
    }

    const auto& model         = helper.get_model();
    const auto  entry_ptr_vec = model.representative_store.get_entry_ptr_vec(model.frame_record_vec.at(0).shape_id);

    ASSERT_EQ(entry_ptr_vec.size(),
              3u);

    EXPECT_EQ(entry_ptr_vec.at(0)->frame_index,
              0u);
    EXPECT_EQ(entry_ptr_vec.at(1)->frame_index,
              1u);
    EXPECT_EQ(entry_ptr_vec.at(2)->frame_index,
              2u);
}

TEST(ModelBuilder, TheCameraIsKeptAsATimeSeriesPerKindOfPass)
{
    const uint8_t          view_marker_array[] = {1, 2, 2, 3};
    Tests::ModelTestHelper helper;
    const auto             swapchain_image     = helper.create_swapchain_image();

    for (const auto& marker : view_marker_array)
    {
        helper.bind          (swapchain_image);
        helper.set_view_matrix(marker);
        helper.draw          ();
        helper.end_frame     ();
    }

    const auto& model = helper.get_model();

    ASSERT_EQ(model.pass_structure_interner.get_n_structures(),
              1u);
    ASSERT_EQ(model.constant_series_map.size(),
              1u);

    const auto& series = model.constant_series_map.begin()->second;

    EXPECT_EQ(series.get_n_updates(),
              4u);
    ASSERT_EQ(series.get_sample_vec().size(),
              3u);
    EXPECT_EQ(series.get_sample_vec().at(0).value_u8_vec.front(),
              1);
    EXPECT_EQ(series.get_sample_vec().at(2).value_u8_vec.front(),
              3);
}

TEST(ModelBuilder, UploadsAreRecordedButDoNotChangeTheShape)
{
    Tests::ModelTestHelper helper;
    const auto             swapchain_image = helper.create_swapchain_image();
    const auto             vertex_buffer   = helper.create_vertex_buffer();

    helper.bind  (swapchain_image);
    helper.upload(vertex_buffer);
    helper.draw  ();
    helper.end_frame();

    helper.bind  (swapchain_image);
    helper.upload(vertex_buffer);
    helper.upload(vertex_buffer);
    helper.upload(vertex_buffer);
    helper.draw  ();
    helper.end_frame();

    const auto& model = helper.get_model();

    EXPECT_EQ(model.frame_record_vec.at(0).shape_id,
              model.frame_record_vec.at(1).shape_id);

    const auto entry_ptr_vec = model.representative_store.get_entry_ptr_vec(model.frame_record_vec.at(0).shape_id);

    ASSERT_GE(entry_ptr_vec.size(),
              1u);
    ASSERT_EQ(entry_ptr_vec.at(0)->detail.transfer_record_vec.size(),
              1u);
    EXPECT_EQ(entry_ptr_vec.at(0)->detail.transfer_record_vec.at(0).kind,
              Analysis::APIDumpAnalysis::TransferKind::UPLOAD);
}

TEST(ModelBuilder, AnUploadToABufferDoesNotEndThePassThatIsOpen)
{
    Tests::ModelTestHelper helper;
    const auto             swapchain_image = helper.create_swapchain_image();
    const auto             vertex_buffer   = helper.create_vertex_buffer();

    helper.bind  (swapchain_image);
    helper.draw  ();
    helper.upload(vertex_buffer);
    helper.draw  ();
    helper.end_frame();

    EXPECT_EQ(helper.get_model().frame_record_vec.at(0).n_passes,
              1u);
}

TEST(ModelBuilder, AnUploadToWhatThePassRendersToEndsThePass)
{
    Tests::ModelTestHelper helper;
    const auto             texture = helper.create_color_texture();

    helper.bind  (texture);
    helper.draw  ();
    helper.upload(texture);
    helper.draw  ();
    helper.end_frame();

    EXPECT_EQ(helper.get_model().frame_record_vec.at(0).n_passes,
              2u);
}

TEST(ModelBuilder, ADestroyedResourceIsMarkedAndTheFrameGoesOn)
{
    Tests::ModelTestHelper helper;
    const auto             swapchain_image = helper.create_swapchain_image();
    const auto             texture         = helper.create_color_texture();

    helper.bind(swapchain_image);
    helper.draw_textured(texture);
    helper.get_builder().on_destroy_resource(texture);
    helper.end_frame();

    const auto& model = helper.get_model();

    EXPECT_TRUE (model.resource_vec.at(1).is_destroyed);
    EXPECT_FALSE(model.resource_vec.at(0).is_destroyed);
    EXPECT_EQ   (model.frame_record_vec.at(0).n_passes,
                 1u);
}

TEST(ModelBuilder, FramesRecordTheirCallsAndTimes)
{
    Tests::ModelTestHelper helper;
    const auto             swapchain_image = helper.create_swapchain_image(); /* call 0 */

    helper.bind     (swapchain_image);                                         /* call 1 */
    helper.draw     ();                                                        /* call 2 */
    helper.end_frame();                                                        /* call 3 */
    helper.bind     (swapchain_image);                                         /* call 4 */
    helper.end_frame();                                                        /* call 5 */

    const auto& frame_record_vec = helper.get_model().frame_record_vec;

    ASSERT_EQ(frame_record_vec.size(),
              2u);

    EXPECT_EQ(frame_record_vec.at(0).first_call_index,
              0u);
    EXPECT_EQ(frame_record_vec.at(0).last_call_index,
              3u);
    EXPECT_EQ(frame_record_vec.at(1).first_call_index,
              4u);
    EXPECT_EQ(frame_record_vec.at(1).last_call_index,
              5u);
    EXPECT_EQ(frame_record_vec.at(0).frame_index,
              0u);
    EXPECT_EQ(frame_record_vec.at(1).frame_index,
              1u);
    EXPECT_LT(frame_record_vec.at(0).end_timestamp_qpc,
              frame_record_vec.at(1).begin_timestamp_qpc);
}

TEST(ModelBuilder, EveryResourceHasAnAccessRecord)
{
    Tests::ModelTestHelper helper;

    helper.create_color_texture();
    helper.create_depth_buffer();
    helper.create_vertex_buffer();

    EXPECT_EQ(helper.get_model().resource_access_vec.size(),
              helper.get_model().resource_vec.size() );
    EXPECT_EQ(helper.get_model().resource_vec.size(),
              3u);
}

TEST(ModelBuilder, PingPongTargetsAreWrittenBeforeTheyAreReadInEachFrame)
{
    Tests::ModelTestHelper helper;
    const auto             color_a   = helper.create_color_texture();
    const auto             color_b   = helper.create_color_texture();
    const auto             swapchain = helper.create_swapchain_image();

    for (int n_frame = 0;
             n_frame < 3;
           ++n_frame)
    {
        helper.bind(color_a);
        helper.clear(true,
                     false);
        helper.draw();

        helper.bind(color_b);
        helper.draw_textured(color_a);

        helper.bind(swapchain);
        helper.draw_textured(color_b);

        helper.end_frame();
    }

    const auto& access_vec = helper.get_model().resource_access_vec;

    EXPECT_EQ(Analysis::APIDumpAnalysis::ResourceAccessTracker::classify(access_vec.at(Analysis::APIDumpAnalysis::get_resource_id_value(color_a) - 1) ),
              Analysis::APIDumpAnalysis::ResourceAccessClass::FRAME_LOCAL);
    EXPECT_EQ(Analysis::APIDumpAnalysis::ResourceAccessTracker::classify(access_vec.at(Analysis::APIDumpAnalysis::get_resource_id_value(color_b) - 1) ),
              Analysis::APIDumpAnalysis::ResourceAccessClass::FRAME_LOCAL);
    EXPECT_EQ(Analysis::APIDumpAnalysis::ResourceAccessTracker::classify(access_vec.at(Analysis::APIDumpAnalysis::get_resource_id_value(swapchain) - 1) ),
              Analysis::APIDumpAnalysis::ResourceAccessClass::UNREAD);
    EXPECT_EQ(access_vec.at(Analysis::APIDumpAnalysis::get_resource_id_value(color_a) - 1).n_frames_read,
              3u);
}

TEST(ModelBuilder, ATargetThatIsSampledBeforeItIsCopiedToIsLoopCarried)
{
    Tests::ModelTestHelper helper;
    const auto             history = helper.create_color_texture();
    const auto             target  = helper.create_color_texture();

    /* Each frame reads the history of the previous one, then replaces it. */
    for (int n_frame = 0;
             n_frame < 3;
           ++n_frame)
    {
        helper.bind(target);
        helper.clear(true,
                     false);
        helper.draw_textured(history);
        helper.copy(target,
                    history);
        helper.end_frame();
    }

    const auto& access_vec = helper.get_model().resource_access_vec;

    EXPECT_EQ(Analysis::APIDumpAnalysis::ResourceAccessTracker::classify(access_vec.at(Analysis::APIDumpAnalysis::get_resource_id_value(history) - 1) ),
              Analysis::APIDumpAnalysis::ResourceAccessClass::LOOP_CARRIED);
    EXPECT_EQ(access_vec.at(Analysis::APIDumpAnalysis::get_resource_id_value(history) - 1).n_frames_loop_carried,
              3u);
    EXPECT_EQ(Analysis::APIDumpAnalysis::ResourceAccessTracker::classify(access_vec.at(Analysis::APIDumpAnalysis::get_resource_id_value(target) - 1) ),
              Analysis::APIDumpAnalysis::ResourceAccessClass::FRAME_LOCAL);
}

TEST(ModelBuilder, BlendingIntoATargetThatIsNotClearedIsLoopCarried)
{
    Tests::ModelTestHelper helper;
    const auto             target = helper.create_color_texture();

    for (int n_frame = 0;
             n_frame < 2;
           ++n_frame)
    {
        helper.bind(target);
        helper.draw_reading_attachments();
        helper.end_frame();
    }

    EXPECT_EQ(Analysis::APIDumpAnalysis::ResourceAccessTracker::classify(helper.get_model().resource_access_vec.at(Analysis::APIDumpAnalysis::get_resource_id_value(target) - 1) ),
              Analysis::APIDumpAnalysis::ResourceAccessClass::LOOP_CARRIED);
}

TEST(ModelBuilder, BlendingIntoATargetThatIsClearedFirstIsNot)
{
    Tests::ModelTestHelper helper;
    const auto             depth  = helper.create_depth_buffer();
    const auto             target = helper.create_color_texture();

    for (int n_frame = 0;
             n_frame < 2;
           ++n_frame)
    {
        helper.bind(target,
                    depth);
        helper.clear(true,
                     true);
        helper.draw_reading_attachments();
        helper.end_frame();
    }

    const auto& access_vec = helper.get_model().resource_access_vec;

    EXPECT_EQ(Analysis::APIDumpAnalysis::ResourceAccessTracker::classify(access_vec.at(Analysis::APIDumpAnalysis::get_resource_id_value(target) - 1) ),
              Analysis::APIDumpAnalysis::ResourceAccessClass::FRAME_LOCAL);
    EXPECT_EQ(Analysis::APIDumpAnalysis::ResourceAccessTracker::classify(access_vec.at(Analysis::APIDumpAnalysis::get_resource_id_value(depth) - 1) ),
              Analysis::APIDumpAnalysis::ResourceAccessClass::FRAME_LOCAL);
    EXPECT_EQ(access_vec.at(Analysis::APIDumpAnalysis::get_resource_id_value(depth) - 1).n_frames_loop_carried,
              0u);
}

TEST(ModelBuilder, ADepthBufferThatIsTestedWithoutBeingClearedIsLoopCarried)
{
    Tests::ModelTestHelper helper;
    const auto             depth  = helper.create_depth_buffer();
    const auto             target = helper.create_color_texture();

    for (int n_frame = 0;
             n_frame < 2;
           ++n_frame)
    {
        helper.bind(target,
                    depth);
        helper.draw_reading_attachments();
        helper.end_frame();
    }

    EXPECT_EQ(Analysis::APIDumpAnalysis::ResourceAccessTracker::classify(helper.get_model().resource_access_vec.at(Analysis::APIDumpAnalysis::get_resource_id_value(depth) - 1) ),
              Analysis::APIDumpAnalysis::ResourceAccessClass::LOOP_CARRIED);
}

TEST(ModelBuilder, ATextureUploadedOnceAndSampledIsStatic)
{
    Tests::ModelTestHelper helper;
    const auto             swapchain = helper.create_swapchain_image();
    const auto             texture   = helper.create_static_texture();

    helper.upload(texture);

    for (int n_frame = 0;
             n_frame < 3;
           ++n_frame)
    {
        helper.bind(swapchain);
        helper.draw_textured(texture);
        helper.end_frame();
    }

    EXPECT_EQ(Analysis::APIDumpAnalysis::ResourceAccessTracker::classify(helper.get_model().resource_access_vec.at(Analysis::APIDumpAnalysis::get_resource_id_value(texture) - 1) ),
              Analysis::APIDumpAnalysis::ResourceAccessClass::STATIC);
}

TEST(ModelBuilder, ATextureUploadedInEachFrameBeforeItIsSampledIsFrameLocal)
{
    Tests::ModelTestHelper helper;
    const auto             swapchain = helper.create_swapchain_image();
    const auto             texture   = helper.create_static_texture();

    for (int n_frame = 0;
             n_frame < 3;
           ++n_frame)
    {
        helper.upload(texture);
        helper.bind(swapchain);
        helper.draw_textured(texture);
        helper.end_frame();
    }

    EXPECT_EQ(Analysis::APIDumpAnalysis::ResourceAccessTracker::classify(helper.get_model().resource_access_vec.at(Analysis::APIDumpAnalysis::get_resource_id_value(texture) - 1) ),
              Analysis::APIDumpAnalysis::ResourceAccessClass::FRAME_LOCAL);
}

TEST(ModelBuilder, ATextureSampledBeforeItIsUploadedInTheSameFrameIsLoopCarried)
{
    Tests::ModelTestHelper helper;
    const auto             swapchain = helper.create_swapchain_image();
    const auto             texture   = helper.create_static_texture();

    for (int n_frame = 0;
             n_frame < 3;
           ++n_frame)
    {
        helper.bind(swapchain);
        helper.draw_textured(texture);
        helper.upload(texture);
        helper.end_frame();
    }

    EXPECT_EQ(Analysis::APIDumpAnalysis::ResourceAccessTracker::classify(helper.get_model().resource_access_vec.at(Analysis::APIDumpAnalysis::get_resource_id_value(texture) - 1) ),
              Analysis::APIDumpAnalysis::ResourceAccessClass::LOOP_CARRIED);
}

TEST(ModelBuilder, ACompletedFrameSaysWhatKindOfFrameItIsAndWhatPassesItWasMadeOf)
{
    Tests::ModelTestHelper helper;
    const auto             target_a  = helper.create_color_texture();
    const auto             swapchain = helper.create_swapchain_image();

    for (int n_frame = 0;
             n_frame < 2;
           ++n_frame)
    {
        helper.bind(target_a);
        helper.clear(true,
                     false);
        helper.draw();
        helper.bind(swapchain);
        helper.draw_textured(target_a);
        helper.end_frame();

        const auto& frame = helper.get_builder().get_completed_frame();

        EXPECT_EQ(frame.frame_index,
                  static_cast<uint64_t>(n_frame) );
        EXPECT_EQ(frame.pass_instance_vec.size(),
                  2u);
        ASSERT_EQ(frame.pass_key_vec.size(),
                  2u);
        EXPECT_EQ(frame.pass_key_vec,
                  helper.get_ordered_pass_keys(0) );
        EXPECT_EQ(frame.shape_key,
                  helper.get_shape_key(0) );
        EXPECT_LE(frame.first_call_index,
                  frame.pass_instance_vec.front().begin_call_index);
        EXPECT_EQ(frame.last_call_index,
                  helper.get_next_call_index() - 1);
    }

    EXPECT_EQ(helper.get_builder().get_n_frames(),
              2u);
}

TEST(ModelBuilder, ABuilderWithoutHistoryKeepsNoFramesButSaysWhatEachWasLike)
{
    Tests::ModelTestHelper helper(false); /* in_keeps_history */
    const auto             swapchain = helper.create_swapchain_image();
    const auto             texture   = helper.create_static_texture();

    for (int n_frame = 0;
             n_frame < 5;
           ++n_frame)
    {
        helper.upload(texture);
        helper.set_view_matrix(static_cast<uint8_t>(n_frame) );
        helper.bind(swapchain);
        helper.draw_textured(texture);
        helper.end_frame();
    }

    const auto& model = helper.get_model();

    EXPECT_TRUE(model.frame_record_vec.empty() );
    EXPECT_TRUE(model.constant_series_map.empty() );
    EXPECT_EQ  (model.pass_structure_interner.get_n_structures(),
                1u);
    EXPECT_EQ  (model.frame_shape_interner.get_n_structures(),
                1u);
    EXPECT_EQ  (model.representative_store.get_entry_ptr_vec(0).size(),
                0u);

    EXPECT_EQ(helper.get_builder().get_n_frames(),
              5u);
    EXPECT_EQ(helper.get_builder().get_completed_frame().frame_index,
              4u);
    EXPECT_EQ(helper.get_builder().get_completed_frame().pass_key_vec.size(),
              1u);
    EXPECT_EQ(model.resource_access_vec.at(Analysis::APIDumpAnalysis::get_resource_id_value(texture) - 1).n_frames_written,
              5u);
}

TEST(ModelBuilder, AFrameIsTheSameKindWithOrWithoutHistory)
{
    Tests::ModelTestHelper with_history(true);    /* in_keeps_history */
    Tests::ModelTestHelper without_history(false); /* in_keeps_history */

    Tests::ModelTestHelper* helper_ptr_array[] = {&with_history,
                                                  &without_history};
    uint64_t                shape_key_u64vec2[2] = {};

    for (size_t n_helper = 0;
                n_helper < 2;
              ++n_helper)
    {
        auto&      helper    = *helper_ptr_array[n_helper];
        const auto swapchain = helper.create_swapchain_image();
        const auto target    = helper.create_color_texture();

        helper.bind(target);
        helper.clear(true,
                     false);
        helper.draw();
        helper.bind(swapchain);
        helper.draw_textured(target);
        helper.end_frame();

        shape_key_u64vec2[n_helper] = helper.get_builder().get_completed_frame().shape_key;
    }

    EXPECT_EQ(shape_key_u64vec2[0],
              shape_key_u64vec2[1]);
    EXPECT_NE(shape_key_u64vec2[0],
              0u);
}

TEST(ModelBuilder, ACallBelongsToThePassWhoseDrawsItIsSetUpFor)
{
    Tests::ModelTestHelper helper;
    const auto             target_a = helper.create_color_texture();
    const auto             target_b = helper.create_color_texture();
    const uint64_t         first    = helper.get_next_call_index();

    helper.bind(target_a);       /* first + 0 */
    helper.set_view_matrix(1);   /* first + 1: for the draws of the pass */
    helper.draw();               /* first + 2 */
    helper.draw();               /* first + 3: the last of the pass */
    helper.bind(target_b);       /* first + 4: ends the pass, and is the setup of the next */
    helper.set_view_matrix(2);   /* first + 5: after the last draw of the first pass, so for the second */
    helper.draw();               /* first + 6 */
    helper.end_frame();          /* first + 7: the present */

    const auto& frame = helper.get_builder().get_completed_frame();

    ASSERT_EQ(frame.pass_instance_vec.size(),
              2u);
    EXPECT_EQ(frame.pass_instance_vec.at(0).last_draw_call_index,
              first + 3);
    EXPECT_EQ(frame.pass_instance_vec.at(1).last_draw_call_index,
              first + 6);

    const uint32_t expected_pass_array[] = {0, 0, 0, 0, 1, 1, 1};

    for (uint64_t n_call = 0;
                  n_call < 7;
                ++n_call)
    {
        uint32_t pass_index = 99;

        ASSERT_TRUE(Analysis::APIDumpAnalysis::ModelBuilder::find_pass_index(frame,
                                                                             first + n_call,
                                                                            &pass_index) ) << n_call;
        EXPECT_EQ  (pass_index,
                    expected_pass_array[n_call]) << n_call;
    }

    /* The present is after the last draw: it is in no pass. */
    uint32_t pass_index = 99;

    EXPECT_FALSE(Analysis::APIDumpAnalysis::ModelBuilder::find_pass_index(frame,
                                                                          first + 7,
                                                                         &pass_index) );
    EXPECT_EQ   (pass_index,
                 99u);
}

TEST(ModelBuilder, EveryDrawOfAFrameIsRecordedWithItsCallItsPassAndTheKeyOfItsStructure)
{
    Tests::ModelTestHelper helper;
    const auto             target_a = helper.create_color_texture();
    const auto             target_b = helper.create_color_texture();
    const uint64_t         first    = helper.get_next_call_index();

    helper.bind(target_a);   /* first + 0 */
    helper.draw(1);          /* first + 1 */
    helper.draw(2);          /* first + 2 */
    helper.bind(target_b);   /* first + 3 */
    helper.draw(1);          /* first + 4 */
    helper.end_frame();

    const auto& frame = helper.get_builder().get_completed_frame();

    ASSERT_EQ(frame.draw_instance_vec.size(),
              3u);

    EXPECT_EQ(frame.draw_instance_vec.at(0).call_index,
              first + 1);
    EXPECT_EQ(frame.draw_instance_vec.at(1).call_index,
              first + 2);
    EXPECT_EQ(frame.draw_instance_vec.at(2).call_index,
              first + 4);
    EXPECT_EQ(frame.draw_instance_vec.at(0).pass_index,
              0u);
    EXPECT_EQ(frame.draw_instance_vec.at(1).pass_index,
              0u);
    EXPECT_EQ(frame.draw_instance_vec.at(2).pass_index,
              1u);

    /* The same kind of draw has the same key wherever it is, and another kind has another. */
    EXPECT_EQ(frame.draw_instance_vec.at(0).structure_key,
              frame.draw_instance_vec.at(2).structure_key);
    EXPECT_NE(frame.draw_instance_vec.at(0).structure_key,
              frame.draw_instance_vec.at(1).structure_key);

    /* The keys are those which the passes of the model list. */
    const auto& structure = helper.get_model().pass_structure_interner.get_structure(frame.pass_instance_vec.at(0).structure_id);

    EXPECT_EQ(std::set<uint64_t>(structure.draw_structure_key_vec.begin(),
                                 structure.draw_structure_key_vec.end() ),
              (std::set<uint64_t>{frame.draw_instance_vec.at(0).structure_key,
                                  frame.draw_instance_vec.at(1).structure_key}) );

    for (const auto& draw : frame.draw_instance_vec)
    {
        EXPECT_NE(helper.get_model().draw_structure_map.find(draw.structure_key),
                  helper.get_model().draw_structure_map.end() );
    }
}

TEST(ModelBuilder, TheDrawsOfAFrameAreOnlyThoseOfThatFrame)
{
    Tests::ModelTestHelper helper;
    const auto             target = helper.create_color_texture();

    helper.bind(target);
    helper.draw();
    helper.draw();
    helper.end_frame();

    EXPECT_EQ(helper.get_builder().get_completed_frame().draw_instance_vec.size(),
              2u);

    helper.bind(target);
    helper.draw();
    helper.end_frame();

    const auto& frame = helper.get_builder().get_completed_frame();

    ASSERT_EQ(frame.draw_instance_vec.size(),
              1u);
    EXPECT_EQ(frame.draw_instance_vec.at(0).pass_index,
              0u);
}

TEST(ModelBuilder, ACompletedFrameTellsWhichPassesRenderToWhatIsPresented)
{
    Tests::ModelTestHelper helper;
    const auto             swapchain_image = helper.create_swapchain_image();
    const auto             target          = helper.create_color_texture();

    helper.bind(target);
    helper.draw();
    helper.bind(swapchain_image);
    helper.draw();
    helper.end_frame();

    const auto& frame = helper.get_builder().get_completed_frame();

    ASSERT_EQ(frame.pass_is_presented_vec.size(),
              frame.pass_instance_vec.size() );
    ASSERT_EQ(frame.pass_instance_vec.size(),
              2u);
    EXPECT_FALSE(frame.pass_is_presented_vec.at(0) );
    EXPECT_TRUE (frame.pass_is_presented_vec.at(1) );
}

TEST(ModelBuilder, AClearIsTheWorkOfAPassWithNoDraws)
{
    Tests::ModelTestHelper helper;
    const auto             target = helper.create_color_texture();

    helper.bind(target);

    const uint64_t clear_call = helper.get_next_call_index();

    helper.clear(true,
                 false);
    helper.end_frame();

    const auto& frame = helper.get_builder().get_completed_frame();

    ASSERT_EQ(frame.pass_instance_vec.size(),
              1u);
    EXPECT_EQ(frame.pass_instance_vec.at(0).last_draw_call_index,
              clear_call);
    EXPECT_EQ(frame.pass_instance_vec.at(0).n_draws,
              0u);
}

TEST(ModelBuilder, ADrawSignatureDoesNotDependOnThePassThatRenderedWhatItSamples)
{
    const auto run_func = [](const uint32_t& in_first_pass_state)
    {
        Tests::ModelTestHelper helper;
        const auto             swapchain_image = helper.create_swapchain_image();
        const auto             texture         = helper.create_color_texture();

        helper.bind         (texture);
        helper.clear        (true,   /* in_clears_color */
                             false); /* in_clears_depth */
        helper.draw         (in_first_pass_state);
        helper.bind         (swapchain_image);
        helper.draw_textured(texture);
        helper.end_frame    ();

        return helper.get_model().pass_structure_interner.get_structure(1);
    };

    const auto first  = run_func(1);
    const auto second = run_func(2);

    ASSERT_EQ(first.draw_signature_vec.size(),
              1u);
    ASSERT_EQ(second.draw_signature_vec.size(),
              1u);

    /* The structure of the draw follows the pass that wrote the texture, which a live converter cannot know yet. */
    EXPECT_NE(first.draw_signature_vec.at(0).structure_key,
              second.draw_signature_vec.at(0).structure_key);
    EXPECT_EQ(first.draw_signature_vec.at(0).signature_key,
              second.draw_signature_vec.at(0).signature_key);
}

TEST(ModelBuilder, ADrawSignatureDependsOnWhatTheDrawRendersTo)
{
    const auto run_func = [](const bool& in_renders_to_texture)
    {
        Tests::ModelTestHelper helper;
        const auto             static_texture  = helper.create_static_texture();
        const auto             swapchain_image = helper.create_swapchain_image();
        const auto             target          = helper.create_color_texture();

        helper.bind         ((in_renders_to_texture) ? target
                                                     : swapchain_image);
        helper.draw_textured(static_texture);
        helper.end_frame    ();

        return helper.get_model().pass_structure_interner.get_structure(0);
    };

    const auto to_texture   = run_func(true);
    const auto to_swapchain = run_func(false);

    ASSERT_EQ(to_texture.draw_signature_vec.size(),
              1u);
    ASSERT_EQ(to_swapchain.draw_signature_vec.size(),
              1u);

    EXPECT_EQ(to_texture.draw_signature_vec.at(0).structure_key,
              to_swapchain.draw_signature_vec.at(0).structure_key);
    EXPECT_NE(to_texture.draw_signature_vec.at(0).signature_key,
              to_swapchain.draw_signature_vec.at(0).signature_key);
}

TEST(ModelBuilder, ADrawSignatureIsKeptOncePerKindOfDrawInAPass)
{
    Tests::ModelTestHelper helper;
    const auto             swapchain_image = helper.create_swapchain_image();

    helper.bind     (swapchain_image);
    helper.draw     (1);
    helper.draw     (1);
    helper.draw     (2);
    helper.end_frame();

    const auto& structure = helper.get_model().pass_structure_interner.get_structure(0);

    ASSERT_EQ(structure.draw_signature_vec.size(),
              2u);
    EXPECT_EQ(structure.draw_structure_key_vec.size(),
              2u);

    /* In the order of the structure keys, which are the keys of the structures of the pass. */
    for (size_t n_draw = 0;
                n_draw < structure.draw_signature_vec.size();
              ++n_draw)
    {
        EXPECT_EQ(structure.draw_signature_vec.at(n_draw).structure_key,
                  structure.draw_structure_key_vec.at(n_draw) );
    }
}

TEST(ModelBuilder, TheSignatureOfTheLastDrawIsKnownAsSoonAsTheDrawIs)
{
    Tests::ModelTestHelper helper;
    const auto             swapchain_image = helper.create_swapchain_image();

    EXPECT_EQ(helper.get_builder().get_last_draw_signature_key(),
              0u);

    helper.bind(swapchain_image);
    helper.draw(1);

    /* The pass is still open: a converter which plays an application as it runs asks now, not when the pass ends. */
    const uint64_t first_signature_key = helper.get_builder().get_last_draw_signature_key();

    helper.draw(2);

    const uint64_t second_signature_key = helper.get_builder().get_last_draw_signature_key();

    helper.end_frame();

    EXPECT_NE(first_signature_key,
              0u);
    EXPECT_NE(first_signature_key,
              second_signature_key);

    /* The same keys the pass ends up with. */
    const auto& structure = helper.get_model().pass_structure_interner.get_structure(0);

    ASSERT_EQ(structure.draw_signature_vec.size(),
              2u);

    for (const auto& draw_signature : structure.draw_signature_vec)
    {
        EXPECT_TRUE(draw_signature.signature_key == first_signature_key ||
                    draw_signature.signature_key == second_signature_key);
    }
}

TEST(ModelBuilder, ADrawIsCountedAtTheDepthOfItsObjectInTheSpaceOfTheCamera)
{
    constexpr size_t N_FLOATS_PER_MATRIX = 16;

    Tests::ModelTestHelper helper;
    const auto             swapchain_image = helper.create_swapchain_image();

    /* The view moves what is seen 4 in front of the camera; the object is 6 from the origin of the world. */
    std::array<float, N_FLOATS_PER_MATRIX> model_f32mat = {1, 0, 0, 0,
                                                           0, 1, 0, 0,
                                                           0, 0, 1, 0,
                                                           0, 0, 6, 1};
    std::array<float, N_FLOATS_PER_MATRIX> view_f32mat  = {1, 0, 0, 0,
                                                           0, 1, 0, 0,
                                                           0, 0, 1, 0,
                                                           0, 0, 4, 1};

    helper.set_matrix(Analysis::APIDumpAnalysis::ConstantSemantic::VIEW_MATRIX,
                      128,
                      view_f32mat);
    helper.set_matrix(Analysis::APIDumpAnalysis::ConstantSemantic::MODEL_MATRIX,
                      16384,
                      model_f32mat);
    helper.bind(swapchain_image);
    helper.draw(0, 30);
    helper.end_frame();

    const auto& model = helper.get_model();

    ASSERT_EQ(model.depth_histogram_map.size(),
              1u);
    EXPECT_EQ(model.depth_histogram_map.begin()->second.get_total_weight(),
              30u);

    /* Within the width of a range, which is a ninth of the depth. */
    EXPECT_NEAR(model.depth_histogram_map.begin()->second.get_depth_at_fraction(0.5),
                10.0f,
                1.0f);
}

TEST(ModelBuilder, ADrawBehindTheCameraOrInAnOrthographicProjectionHasNoDepth)
{
    constexpr size_t N_FLOATS_PER_MATRIX = 16;

    std::array<float, N_FLOATS_PER_MATRIX> behind_f32mat       = {1, 0, 0, 0,
                                                                  0, 1, 0, 0,
                                                                  0, 0, 1, 0,
                                                                  0, 0, -5, 1};
    std::array<float, N_FLOATS_PER_MATRIX> identity_f32mat     = {1, 0, 0, 0,
                                                                  0, 1, 0, 0,
                                                                  0, 0, 1, 0,
                                                                  0, 0, 0, 1};
    std::array<float, N_FLOATS_PER_MATRIX> orthographic_f32mat = identity_f32mat;
    std::array<float, N_FLOATS_PER_MATRIX> view_f32mat         = {1, 0, 0, 0,
                                                                  0, 1, 0, 0,
                                                                  0, 0, 1, 0,
                                                                  0, 0, 4, 1};

    /* Orthographic: the last column is (0, 0, 0, 1). */
    orthographic_f32mat.at(15) = 1.0f;

    {
        Tests::ModelTestHelper helper;
        const auto             swapchain_image = helper.create_swapchain_image();

        helper.set_matrix(Analysis::APIDumpAnalysis::ConstantSemantic::VIEW_MATRIX,
                          128,
                          view_f32mat);
        helper.set_matrix(Analysis::APIDumpAnalysis::ConstantSemantic::MODEL_MATRIX,
                          16384,
                          behind_f32mat);
        helper.bind(swapchain_image);
        helper.draw(0, 30);
        helper.end_frame();

        EXPECT_TRUE(helper.get_model().depth_histogram_map.empty() );
    }

    {
        Tests::ModelTestHelper helper;
        const auto             swapchain_image = helper.create_swapchain_image();

        helper.set_matrix(Analysis::APIDumpAnalysis::ConstantSemantic::PROJECTION_MATRIX,
                          192,
                          orthographic_f32mat);
        helper.set_matrix(Analysis::APIDumpAnalysis::ConstantSemantic::VIEW_MATRIX,
                          128,
                          view_f32mat);
        helper.bind(swapchain_image);
        helper.draw(0, 30);
        helper.end_frame();

        EXPECT_TRUE(helper.get_model().depth_histogram_map.empty() );
    }
}

TEST(ModelBuilder, WithoutHistoryNoDepthIsKept)
{
    constexpr size_t N_FLOATS_PER_MATRIX = 16;

    Tests::ModelTestHelper                 helper(false); /* in_keeps_history */
    const auto                             swapchain_image = helper.create_swapchain_image();
    std::array<float, N_FLOATS_PER_MATRIX> view_f32mat     = {1, 0, 0, 0,
                                                              0, 1, 0, 0,
                                                              0, 0, 1, 0,
                                                              0, 0, 4, 1};

    helper.set_matrix(Analysis::APIDumpAnalysis::ConstantSemantic::VIEW_MATRIX,
                      128,
                      view_f32mat);
    helper.bind(swapchain_image);
    helper.draw(0, 30);
    helper.end_frame();

    EXPECT_TRUE(helper.get_model().depth_histogram_map.empty() );
}

TEST(ModelBuilder, TheSurfacesOfADrawAreCountedAtTheDepthOfTheirVerticesInTheSpaceOfTheCamera)
{
    constexpr size_t N_FLOATS_PER_MATRIX = 16;

    Tests::ModelTestHelper                 helper;
    const auto                             swapchain_image = helper.create_swapchain_image();
    std::array<float, N_FLOATS_PER_MATRIX> view_f32mat     = {1, 0, 0, 0,
                                                              0, 1, 0, 0,
                                                              0, 0, 1, 0,
                                                              0, 0, 4, 1};

    /* The object is at the origin of the world, which is 4 in front of the camera: its vertices are 2 and 10 further. */
    helper.set_matrix(Analysis::APIDumpAnalysis::ConstantSemantic::VIEW_MATRIX,
                      128,
                      view_f32mat);
    helper.bind(swapchain_image);
    helper.draw_with_geometry({{0.0f, 0.0f, 2.0f},
                               {0.0f, 0.0f, 10.0f}},
                              30);
    helper.end_frame();

    const auto& model = helper.get_model();

    ASSERT_EQ(model.surface_depth_histogram_map.size(),
              1u);

    const auto& histogram = model.surface_depth_histogram_map.begin()->second;

    /* The weight of the draw is shared by the vertices that were looked at. */
    EXPECT_EQ(histogram.get_total_weight(),
              30u);
    EXPECT_NEAR(histogram.get_depth_at_fraction(0.25),
                6.0f,
                0.6f);
    EXPECT_NEAR(histogram.get_depth_at_fraction(0.75),
                14.0f,
                1.4f);
}

TEST(ModelBuilder, AVertexBehindTheCameraOrOutsideTheViewIsNotCounted)
{
    constexpr size_t N_FLOATS_PER_MATRIX = 16;

    Tests::ModelTestHelper                 helper;
    const auto                             swapchain_image  = helper.create_swapchain_image();
    std::array<float, N_FLOATS_PER_MATRIX> projection_f32mat = {1, 0, 0,     0,
                                                                0, 1, 0,     0,
                                                                0, 0, 1,     1,
                                                                0, 0, -0.1f, 0}; /* the near plane is at 0.1 */
    std::array<float, N_FLOATS_PER_MATRIX> view_f32mat       = {1, 0, 0, 0,
                                                                0, 1, 0, 0,
                                                                0, 0, 1, 0,
                                                                0, 0, 0, 1};

    helper.set_matrix(Analysis::APIDumpAnalysis::ConstantSemantic::PROJECTION_MATRIX,
                      192,
                      projection_f32mat);
    helper.set_matrix(Analysis::APIDumpAnalysis::ConstantSemantic::VIEW_MATRIX,
                      128,
                      view_f32mat);
    helper.bind(swapchain_image);

    /* Behind the camera, in front of the near plane, far off to the side, and the one that is seen. */
    helper.draw_with_geometry({{0.0f,  0.0f, -3.0f},
                               {0.0f,  0.0f,  0.05f},
                               {50.0f, 0.0f,  2.0f},
                               {0.0f,  0.0f,  5.0f}},
                              4);
    helper.end_frame();

    const auto& model = helper.get_model();

    ASSERT_EQ(model.surface_depth_histogram_map.size(),
              1u);
    EXPECT_EQ(model.surface_depth_histogram_map.begin()->second.get_total_weight(),
              1u);
    EXPECT_NEAR(model.surface_depth_histogram_map.begin()->second.get_depth_at_fraction(0.5),
                5.0f,
                0.5f);
}

TEST(ModelBuilder, PreTransformedVerticesAreCountedByTheReciprocalOfTheirDepthAndNoMatrixIsAsked)
{
    Tests::ModelTestHelper helper;
    const auto             swapchain_image = helper.create_swapchain_image();

    helper.bind        (swapchain_image);
    helper.set_viewport(0, 0, 640, 480);

    /* A depth of 10, one of 20, one off the picture, and one that is not in front of the camera (its reciprocal is not above zero). */
    helper.draw_with_screen_geometry({{100.0f, 100.0f,  0.1f},
                                      {200.0f, 200.0f,  0.05f},
                                      {700.0f, 100.0f,  0.2f},
                                      {300.0f, 300.0f, -1.0f}},
                                     40);
    helper.end_frame();

    const auto& model = helper.get_model();

    ASSERT_EQ(model.surface_depth_histogram_map.size(),
              1u);

    const auto& histogram = model.surface_depth_histogram_map.begin()->second;

    /* The two in view have the share of the draw that each of four has. */
    EXPECT_EQ(histogram.get_total_weight(),
              20u);
    EXPECT_NEAR(histogram.get_depth_at_fraction(0.25),
                10.0f,
                1.0f);
    EXPECT_NEAR(histogram.get_depth_at_fraction(0.75),
                20.0f,
                2.0f);

    /* And the draw as a whole is where the middle of what was seen is. */
    ASSERT_EQ(model.depth_histogram_map.size(),
              1u);
    EXPECT_EQ(model.depth_histogram_map.begin()->second.get_total_weight(),
              40u);
}

TEST(ModelBuilder, AFrameHoldsTheCoverKeyOfEachOfItsDraws)
{
    Tests::ModelTestHelper helper;
    const auto             swapchain_image = helper.create_swapchain_image();

    helper.bind(swapchain_image);
    helper.draw(1,
                3);
    const uint64_t first_cover_key = helper.get_builder().get_last_draw_cover_key();

    helper.draw(2,
                6);
    const uint64_t second_cover_key = helper.get_builder().get_last_draw_cover_key();

    helper.end_frame();

    const auto& draw_vec = helper.get_builder().get_completed_frame().draw_instance_vec;

    ASSERT_EQ(draw_vec.size(),
              2u);
    EXPECT_EQ(draw_vec.at(0).cover_key,
              first_cover_key);
    EXPECT_EQ(draw_vec.at(1).cover_key,
              second_cover_key);

    /* A draw is told by its signature and its size. */
    EXPECT_NE(first_cover_key,
              second_cover_key);
}

TEST(ModelBuilder, ADrawOfPreTransformedVerticesWhichReachTheEdgesOfTheViewportCoversTheView)
{
    {
        Tests::ModelTestHelper helper;
        const auto             swapchain_image = helper.create_swapchain_image();

        helper.bind        (swapchain_image);
        helper.set_viewport(0, 0, 640, 480);
        helper.draw_with_screen_geometry({{0.0f,   0.0f,   0.2f},
                                          {640.0f, 0.0f,   0.2f},
                                          {0.0f,   480.0f, 0.2f},
                                          {640.0f, 480.0f, 0.2f}},
                                         4);
        helper.end_frame();

        ASSERT_EQ(helper.get_model().covering_draw_key_map.size(),
                  1u);
    }

    /* A flat quad from one side to the other covers the picture when it has bars above and below, and a mesh from one side to the other (a floor)
     * does not: it is not at one depth.
     */
    {
        Tests::ModelTestHelper flat;
        Tests::ModelTestHelper floor;

        for (auto* helper_ptr : {&flat, &floor})
        {
            helper_ptr->bind        (helper_ptr->create_swapchain_image() );
            helper_ptr->set_viewport(0, 0, 640, 480);
        }

        flat.draw_with_screen_geometry({{0.0f,   40.0f,  0.2f},
                                        {640.0f, 40.0f,  0.2f},
                                        {0.0f,   440.0f, 0.2f},
                                        {640.0f, 440.0f, 0.2f}},
                                       6);
        floor.draw_with_screen_geometry({{0.0f,   300.0f, 0.05f},
                                         {640.0f, 300.0f, 0.05f},
                                         {0.0f,   440.0f, 0.5f},
                                         {640.0f, 440.0f, 0.5f}},
                                        6);
        flat.end_frame();
        floor.end_frame();

        EXPECT_EQ(flat.get_model().covering_draw_key_map.size(),
                  1u);
        EXPECT_TRUE(floor.get_model().covering_draw_key_map.empty() );
    }

    /* Not all of it. */
    {
        Tests::ModelTestHelper helper;
        const auto             swapchain_image = helper.create_swapchain_image();

        helper.bind        (swapchain_image);
        helper.set_viewport(0, 0, 640, 480);
        helper.draw_with_screen_geometry({{100.0f, 100.0f, 0.2f},
                                          {540.0f, 100.0f, 0.2f},
                                          {100.0f, 380.0f, 0.2f},
                                          {540.0f, 380.0f, 0.2f}},
                                         4);
        helper.end_frame();

        EXPECT_TRUE(helper.get_model().covering_draw_key_map.empty() );
    }
}

TEST(ModelBuilder, ADrawWhoseVerticesAreNotKnownHasNoSurfaceDepth)
{
    constexpr size_t N_FLOATS_PER_MATRIX = 16;

    Tests::ModelTestHelper                 helper;
    const auto                             swapchain_image = helper.create_swapchain_image();
    std::array<float, N_FLOATS_PER_MATRIX> view_f32mat     = {1, 0, 0, 0,
                                                              0, 1, 0, 0,
                                                              0, 0, 1, 0,
                                                              0, 0, 4, 1};

    helper.set_matrix(Analysis::APIDumpAnalysis::ConstantSemantic::VIEW_MATRIX,
                      128,
                      view_f32mat);
    helper.bind(swapchain_image);
    helper.draw(0, 30);
    helper.end_frame();

    EXPECT_FALSE(helper.get_model().depth_histogram_map.empty() );
    EXPECT_TRUE (helper.get_model().surface_depth_histogram_map.empty() );
}

TEST(ModelBuilder, VerticesAreWantedOnlyWhenTheHistoryIsKept)
{
    Tests::ModelTestHelper with_history;
    Tests::ModelTestHelper without_history(false); /* in_keeps_history */

    EXPECT_TRUE (with_history.get_builder().wants_geometry() );
    EXPECT_FALSE(without_history.get_builder().wants_geometry() );
}

TEST(ModelBuilder, ADrawWhoseVerticesReachAllFourEdgesOfThePictureCoversTheView)
{
    constexpr size_t N_FLOATS_PER_MATRIX = 16;

    std::array<float, N_FLOATS_PER_MATRIX> projection_f32mat = {1, 0, 0,     0,
                                                                0, 1, 0,     0,
                                                                0, 0, 1,     1,
                                                                0, 0, -0.1f, 0};
    std::array<float, N_FLOATS_PER_MATRIX> view_f32mat       = {1, 0, 0, 0,
                                                                0, 1, 0, 0,
                                                                0, 0, 1, 0,
                                                                0, 0, 0, 1};

    /* A quad of the size of the picture at depth 2: its corners are at plus and minus 2 in x and y. */
    {
        Tests::ModelTestHelper helper;
        const auto             swapchain_image = helper.create_swapchain_image();

        helper.set_matrix(Analysis::APIDumpAnalysis::ConstantSemantic::PROJECTION_MATRIX,
                          192,
                          projection_f32mat);
        helper.set_matrix(Analysis::APIDumpAnalysis::ConstantSemantic::VIEW_MATRIX,
                          128,
                          view_f32mat);
        helper.bind(swapchain_image);
        helper.draw_with_geometry({{-2.0f, -2.0f, 2.0f},
                                   { 2.0f, -2.0f, 2.0f},
                                   {-2.0f,  2.0f, 2.0f},
                                   { 2.0f,  2.0f, 2.0f}},
                                  4);
        helper.end_frame();

        ASSERT_EQ(helper.get_model().covering_draw_key_map.size(),
                  1u);
        EXPECT_EQ(helper.get_model().covering_draw_key_map.begin()->second.size(),
                  1u);
    }

    /* A quad of a part of it, and a quad that covers the width but not the height: neither is. */
    {
        Tests::ModelTestHelper helper;
        const auto             swapchain_image = helper.create_swapchain_image();

        helper.set_matrix(Analysis::APIDumpAnalysis::ConstantSemantic::PROJECTION_MATRIX,
                          192,
                          projection_f32mat);
        helper.set_matrix(Analysis::APIDumpAnalysis::ConstantSemantic::VIEW_MATRIX,
                          128,
                          view_f32mat);
        helper.bind(swapchain_image);
        helper.draw_with_geometry({{-0.5f, -0.5f, 2.0f},
                                   { 0.5f, -0.5f, 2.0f},
                                   {-0.5f,  0.5f, 2.0f},
                                   { 0.5f,  0.5f, 2.0f}},
                                  4);
        helper.draw_with_geometry({{-2.0f, -0.5f, 2.0f},
                                   { 2.0f, -0.5f, 2.0f},
                                   {-2.0f,  0.5f, 2.0f},
                                   { 2.0f,  0.5f, 2.0f}},
                                  4);
        helper.end_frame();

        EXPECT_TRUE(helper.get_model().covering_draw_key_map.empty() );
    }
}

TEST(ModelBuilder, TheDrawsOfOneSignatureAreToldApartBySizeAndAKeyThatHasCoveredTheViewIsKept)
{
    constexpr size_t N_FLOATS_PER_MATRIX = 16;

    Tests::ModelTestHelper                 helper;
    const auto                             swapchain_image  = helper.create_swapchain_image();
    std::array<float, N_FLOATS_PER_MATRIX> projection_f32mat = {1, 0, 0,     0,
                                                                0, 1, 0,     0,
                                                                0, 0, 1,     1,
                                                                0, 0, -0.1f, 0};
    std::array<float, N_FLOATS_PER_MATRIX> view_f32mat       = {1, 0, 0, 0,
                                                                0, 1, 0, 0,
                                                                0, 0, 1, 0,
                                                                0, 0, 0, 1};

    helper.set_matrix(Analysis::APIDumpAnalysis::ConstantSemantic::PROJECTION_MATRIX,
                      192,
                      projection_f32mat);
    helper.set_matrix(Analysis::APIDumpAnalysis::ConstantSemantic::VIEW_MATRIX,
                      128,
                      view_f32mat);
    helper.bind(swapchain_image);

    /* The same state and buffers: a quad of the size of the picture of 4 vertices, and a small one of 6. */
    helper.draw_with_geometry({{-2.0f, -2.0f, 2.0f},
                               { 2.0f, -2.0f, 2.0f},
                               {-2.0f,  2.0f, 2.0f},
                               { 2.0f,  2.0f, 2.0f}},
                              4);

    const uint64_t covering_key = helper.get_builder().get_last_draw_cover_key();

    helper.draw_with_geometry({{-0.5f, -0.5f, 2.0f},
                               { 0.5f, -0.5f, 2.0f},
                               {-0.5f,  0.5f, 2.0f},
                               { 0.5f,  0.5f, 2.0f}},
                              6);

    const uint64_t partial_key = helper.get_builder().get_last_draw_cover_key();

    EXPECT_NE(covering_key,
              partial_key);
    ASSERT_EQ(helper.get_model().covering_draw_key_map.size(),
              0u); /* a pass is added to the model when it is over */

    /* The same size as the covering one, and it does not cover: the key stays, for a quad of that size is the whole picture at times. */
    helper.draw_with_geometry({{-0.5f, -0.5f, 2.0f},
                               { 0.5f, -0.5f, 2.0f},
                               {-0.5f,  0.5f, 2.0f},
                               { 0.5f,  0.5f, 2.0f}},
                              4);
    helper.end_frame();

    EXPECT_EQ(helper.get_builder().get_last_draw_cover_key(),
              covering_key);
    ASSERT_EQ(helper.get_model().covering_draw_key_map.size(),
              1u);
    EXPECT_EQ(helper.get_model().covering_draw_key_map.begin()->second,
              std::set<uint64_t>({covering_key}) );
}
