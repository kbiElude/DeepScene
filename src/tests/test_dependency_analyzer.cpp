/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Checks of what the analysis of the dependencies tells of accesses made up by hand: what a frame uses of what the frames before it left in a
 * resource, and which frame left it.
 */
#include <gtest/gtest.h>
#include "analysis/api_dump_dependencies/dependency_analyzer.h"
#include "dependency_scene.h"
#include <vector>

namespace
{
    using namespace Analysis::APIDumpAnalysis;
    using namespace Analysis::APIDumpDependencies;

    typedef Tests::DependencyScene Scene;
}

TEST(DependencyAnalyzer, ATextureWhichIsWrittenOnceAndThenSampledIsStatic)
{
    Scene scene;
    const auto texture = scene.create_resource();

    scene.access(texture,
                 AccessRole::UPLOAD_DESTINATION,
                 AccessMode::WRITE,
                 AccessCoverage::FULL);
    scene.end_frame();

    for (uint32_t n_frame = 1;
                  n_frame <= 5;
                ++n_frame)
    {
        scene.sample(texture);
        scene.end_frame();
    }

    const auto  result      = scene.analyze();
    const auto& dependencies = result.resource_vec.at(0);

    EXPECT_EQ(result.n_frames,
              6u);
    EXPECT_EQ(dependencies.summary.dependency_class,
              DependencyClass::STATIC);
    EXPECT_EQ(dependencies.summary.n_frames_used,
              6u);
    EXPECT_EQ(dependencies.summary.n_frames_using_earlier_content,
              5u);
    EXPECT_EQ(dependencies.summary.longest_carry_n_frames,
              5u);

    /* The frames which sample it use what the first one wrote. */
    ASSERT_EQ(dependencies.version_vec.size(),
              1u);
    EXPECT_TRUE(dependencies.version_vec.at(0).has_writer);
    EXPECT_EQ(dependencies.version_vec.at(0).writer_frame_index,
              0u);
    ASSERT_EQ(dependencies.version_vec.at(0).consumer_range_vec.size(),
              1u);
    EXPECT_EQ(dependencies.version_vec.at(0).consumer_range_vec.at(0).first_frame_index,
              1u);
    EXPECT_EQ(dependencies.version_vec.at(0).consumer_range_vec.at(0).last_frame_index,
              5u);
    EXPECT_FALSE(dependencies.version_vec.at(0).keeps_earlier_content);

    /* Two kinds of frames: the one which writes, and those which read. */
    EXPECT_EQ(dependencies.use_run_vec.size(),
              2u);
}

TEST(DependencyAnalyzer, ATargetWhichIsClearedAndDrawnToAndSampledInEachFrameIsFrameLocal)
{
    Scene scene;
    const auto target = scene.create_resource();

    for (uint32_t n_frame = 0;
                  n_frame < 10;
                ++n_frame)
    {
        scene.clear_fully(target);
        scene.draw_to    (target);
        scene.sample     (target);
        scene.end_frame  ();
    }

    const auto  result       = scene.analyze();
    const auto& dependencies = result.resource_vec.at(0);

    EXPECT_EQ(dependencies.summary.dependency_class,
              DependencyClass::FRAME_LOCAL);
    EXPECT_EQ(dependencies.summary.n_frames_using_earlier_content,
              0u);
    EXPECT_EQ(dependencies.summary.n_frames_using_earlier_content_possibly,
              0u);

    /* Ten frames which do the same are one run, and nobody uses what a frame left. */
    ASSERT_EQ(dependencies.use_run_vec.size(),
              1u);
    EXPECT_EQ(dependencies.use_run_vec.at(0).last_frame_index,
              9u);

    for (const auto& version : dependencies.version_vec)
    {
        EXPECT_EQ(version.n_consumer_frames,
                  0u);
    }
}

TEST(DependencyAnalyzer, ATextureWhichIsSampledBeforeItIsWrittenInEachFrameIsCarried)
{
    Scene scene;
    const auto feedback = scene.create_resource();

    for (uint32_t n_frame = 0;
                  n_frame < 6;
                ++n_frame)
    {
        scene.sample (feedback);
        scene.clear_fully(feedback);
        scene.draw_to(feedback);
        scene.end_frame();
    }

    const auto  result       = scene.analyze();
    const auto& dependencies = result.resource_vec.at(0);

    EXPECT_EQ(dependencies.summary.dependency_class,
              DependencyClass::CARRIED);
    EXPECT_EQ(dependencies.summary.n_frames_carrying,
              6u);
    EXPECT_EQ(dependencies.summary.longest_carry_n_frames,
              1u);

    /* Frame 0 uses what the application made it with; every other frame uses what the one before wrote. */
    ASSERT_EQ(dependencies.version_vec.size(),
              7u);
    EXPECT_FALSE(dependencies.version_vec.at(0).has_writer);
    EXPECT_EQ(dependencies.version_vec.at(0).n_consumer_frames,
              1u);
    EXPECT_EQ(dependencies.version_vec.at(3).writer_frame_index,
              2u);
    ASSERT_EQ(dependencies.version_vec.at(3).consumer_range_vec.size(),
              1u);
    EXPECT_EQ(dependencies.version_vec.at(3).consumer_range_vec.at(0).first_frame_index,
              3u);
}

TEST(DependencyAnalyzer, ADrawWhichBlendsIntoATargetUsesWhatIsInIt)
{
    Scene scene;
    const auto cleared = scene.create_resource();
    const auto kept    = scene.create_resource();

    for (uint32_t n_frame = 0;
                  n_frame < 4;
                ++n_frame)
    {
        scene.clear_fully(cleared);

        for (const auto& id : {cleared, kept})
        {
            scene.access(id,
                         AccessRole::COLOR_ATTACHMENT,
                         AccessMode::READ_WRITE);
        }

        scene.next_call();
        scene.end_frame();
    }

    const auto result = scene.analyze();

    EXPECT_EQ(result.resource_vec.at(0).summary.dependency_class,
              DependencyClass::FRAME_LOCAL);
    EXPECT_EQ(result.resource_vec.at(1).summary.dependency_class,
              DependencyClass::CARRIED);
}

TEST(DependencyAnalyzer, WhatIsReadAfterAWriteWhichMayNotReachAllOfItMayBeOld)
{
    Scene scene;
    const auto drawn   = scene.create_resource();
    const auto partial = scene.create_resource();

    for (uint32_t n_frame = 0;
                  n_frame < 4;
                ++n_frame)
    {
        scene.draw_to(drawn);
        scene.sample (drawn);

        scene.access(partial,
                     AccessRole::CLEAR_TARGET,
                     AccessMode::WRITE,
                     AccessCoverage::PARTIAL);
        scene.next_call();
        scene.sample(partial);

        scene.end_frame();
    }

    const auto result = scene.analyze();

    EXPECT_EQ(result.resource_vec.at(0).summary.dependency_class,
              DependencyClass::CARRIED_POSSIBLY);
    EXPECT_EQ(result.resource_vec.at(0).summary.n_frames_carrying,
              0u);
    EXPECT_EQ(result.resource_vec.at(0).summary.n_frames_carrying_possibly,
              4u);
    EXPECT_EQ(result.resource_vec.at(1).summary.dependency_class,
              DependencyClass::CARRIED_POSSIBLY);

    /* The version which a frame leaves is not all of the resource: what was there before is partly still there. */
    EXPECT_TRUE(result.resource_vec.at(0).version_vec.back().keeps_earlier_content);
    EXPECT_EQ(result.resource_vec.at(0).version_vec.at(1).n_possible_consumer_frames,
              1u);
}

TEST(DependencyAnalyzer, ATargetWhichIsSampledWhileItIsRenderedToIsToldSo)
{
    Scene scene;
    const auto cleared = scene.create_resource();
    const auto raw     = scene.create_resource();

    for (uint32_t n_frame = 0;
                  n_frame < 3;
                ++n_frame)
    {
        scene.clear_fully(cleared);

        for (const auto& id : {cleared, raw})
        {
            scene.access(id,
                         AccessRole::SAMPLED,
                         AccessMode::READ);
            scene.access(id,
                         AccessRole::COLOR_ATTACHMENT,
                         AccessMode::WRITE);
        }

        scene.next_call();
        scene.end_frame();
    }

    const auto result = scene.analyze();

    EXPECT_EQ(result.resource_vec.at(0).summary.n_frames_self_sampled,
              3u);
    EXPECT_EQ(result.resource_vec.at(1).summary.n_frames_self_sampled,
              3u);

    /* After a clear of all of it, what it reads is its own; without one, what the frame before left. */
    EXPECT_EQ(result.resource_vec.at(0).summary.dependency_class,
              DependencyClass::FRAME_LOCAL);
    EXPECT_EQ(result.resource_vec.at(1).summary.dependency_class,
              DependencyClass::CARRIED);
}

TEST(DependencyAnalyzer, AWriteOfOneMipLevelDoesNotReplaceAllOfTheResource)
{
    Scene scene;
    const auto mipped = scene.create_resource(3);
    const auto single = scene.create_resource(1);

    SubresourceRange first_level;

    first_level.n_layers     = 1;
    first_level.n_mip_levels = 1;

    SubresourceRange all_levels;

    all_levels.n_layers     = 1;
    all_levels.n_mip_levels = 3;

    for (uint32_t n_frame = 0;
                  n_frame < 3;
                ++n_frame)
    {
        scene.access(mipped,
                     AccessRole::COLOR_ATTACHMENT,
                     AccessMode::WRITE,
                     AccessCoverage::FULL,
                     first_level);
        scene.access(single,
                     AccessRole::COLOR_ATTACHMENT,
                     AccessMode::WRITE,
                     AccessCoverage::FULL,
                     first_level);
        scene.next_call();
        scene.sample(mipped);
        scene.sample(single);
        scene.end_frame();
    }

    /* What is read after a write of one level of three may be of the others, which are old. The single level is all of what the resource is. */
    const auto result = scene.analyze();

    EXPECT_EQ(result.resource_vec.at(0).summary.n_frames_using_earlier_content_possibly,
              3u);
    EXPECT_EQ(result.resource_vec.at(1).summary.n_frames_using_earlier_content_possibly,
              0u);
    EXPECT_EQ(result.resource_vec.at(1).summary.n_frames_using_earlier_content,
              0u);

    /* All the levels together are all of it. */
    Scene whole_scene;
    const auto whole = whole_scene.create_resource(3);

    whole_scene.access(whole,
                       AccessRole::COLOR_ATTACHMENT,
                       AccessMode::WRITE,
                       AccessCoverage::FULL,
                       all_levels);
    whole_scene.next_call();
    whole_scene.sample(whole);
    whole_scene.end_frame();

    EXPECT_EQ(whole_scene.analyze().resource_vec.at(0).summary.dependency_class,
              DependencyClass::FRAME_LOCAL);
}

TEST(DependencyAnalyzer, FramesWhichDoNotTouchAResourceBreakItsRunsAndItsTimesAreRemembered)
{
    Scene scene;
    const auto texture = scene.create_resource();
    const auto unused  = scene.create_resource();

    scene.sample    (texture);
    scene.end_frame ();
    scene.end_frame ();
    scene.sample    (texture);
    scene.end_frame ();

    const auto  result       = scene.analyze();
    const auto& dependencies = result.resource_vec.at(0);

    EXPECT_EQ(dependencies.use_run_vec.size(),
              2u);
    EXPECT_EQ(dependencies.summary.first_frame_index,
              0u);
    EXPECT_EQ(dependencies.summary.last_frame_index,
              2u);
    EXPECT_EQ(result.resource_vec.at(1).summary.dependency_class,
              DependencyClass::UNUSED);
    EXPECT_EQ(result.n_frames,
              3u);

    /* The frames which use what the application made it with: the version without a writer has its consumers. */
    ASSERT_EQ(dependencies.version_vec.size(),
              1u);
    EXPECT_FALSE(dependencies.version_vec.at(0).has_writer);
    EXPECT_EQ(dependencies.version_vec.at(0).consumer_range_vec.size(),
              2u);
}

TEST(DependencyAnalyzer, AccessesAfterTheLastEndedFrameAreOfAFrameWhichDidNotEnd)
{
    Scene scene;
    const auto texture = scene.create_resource();

    scene.sample   (texture);
    scene.end_frame();
    scene.sample   (texture);

    const auto result = scene.analyze();

    EXPECT_EQ(result.n_frames,
              2u);
    EXPECT_EQ(result.resource_vec.at(0).summary.n_frames_used,
              2u);
}

TEST(DependencyAnalyzer, ThreadsGiveTheResultOfOne)
{
    constexpr uint32_t N_FRAMES    = 40;
    constexpr uint32_t N_RESOURCES = 300;

    Scene                   scene;
    std::vector<ResourceId> resource_id_vec;

    for (uint32_t n_resource = 0;
                  n_resource < N_RESOURCES;
                ++n_resource)
    {
        resource_id_vec.push_back(scene.create_resource() );
    }

    for (uint32_t n_frame = 0;
                  n_frame < N_FRAMES;
                ++n_frame)
    {
        for (uint32_t n_resource = 0;
                      n_resource < N_RESOURCES;
                    ++n_resource)
        {
            /* Every kind of use, spread over the resources and the frames. */
            switch ((n_resource + n_frame / 7) % 5)
            {
                case 0:
                {
                    scene.sample(resource_id_vec.at(n_resource) );

                    break;
                }

                case 1:
                {
                    scene.clear_fully(resource_id_vec.at(n_resource) );
                    scene.draw_to    (resource_id_vec.at(n_resource) );
                    scene.sample     (resource_id_vec.at(n_resource) );

                    break;
                }

                case 2:
                {
                    scene.sample (resource_id_vec.at(n_resource) );
                    scene.draw_to(resource_id_vec.at(n_resource) );

                    break;
                }

                case 3:
                {
                    scene.draw_to(resource_id_vec.at(n_resource) );

                    break;
                }

                default:
                {
                    break;
                }
            }
        }

        scene.end_frame();
    }

    const auto single = scene.analyze(1);
    const auto many   = scene.analyze(8);

    ASSERT_EQ(single.resource_vec.size(),
              many.resource_vec.size() );

    for (size_t n_resource = 0;
                n_resource < single.resource_vec.size();
              ++n_resource)
    {
        const auto& a = single.resource_vec.at(n_resource);
        const auto& b = many.resource_vec.at  (n_resource);

        EXPECT_EQ(a.summary.dependency_class,
                  b.summary.dependency_class)  << "resource " << n_resource;
        EXPECT_EQ(a.summary.n_frames_carrying,
                  b.summary.n_frames_carrying) << "resource " << n_resource;
        EXPECT_EQ(a.summary.n_frames_used,
                  b.summary.n_frames_used)     << "resource " << n_resource;
        EXPECT_EQ(a.use_run_vec.size(),
                  b.use_run_vec.size() )       << "resource " << n_resource;
        EXPECT_EQ(a.version_vec.size(),
                  b.version_vec.size() )       << "resource " << n_resource;
    }
}

TEST(DependencyAnalyzer, TheExtentOfWritesIsTheRectangleThatHoldsAllOfThemWhenEachSaysWhereItLanded)
{
    Scene scene;
    const auto texture = scene.create_resource();

    scene.copy_to(texture, 0, 0, 10, 10);
    scene.sample (texture);
    scene.end_frame();

    scene.copy_to(texture, 20, 5, 10, 10);
    scene.sample (texture);
    scene.end_frame();

    const auto  result       = scene.analyze();
    const auto& dependencies = result.resource_vec.at(0);

    ASSERT_TRUE(dependencies.written_extent.is_known);

    EXPECT_EQ(dependencies.written_extent.x,
              0u);
    EXPECT_EQ(dependencies.written_extent.y,
              0u);
    EXPECT_EQ(dependencies.written_extent.width,
              30u);
    EXPECT_EQ(dependencies.written_extent.height,
              15u);

    /* What each frame wrote is kept with the version it made: the one before the first has no writer. */
    ASSERT_EQ(dependencies.version_vec.size(),
              3u);
    ASSERT_EQ(dependencies.version_vec.at(2).written_rectangle_vec.size(),
              1u);

    EXPECT_EQ(dependencies.version_vec.at(1).written_rectangle_vec.at(0).x,
              0u);
    EXPECT_EQ(dependencies.version_vec.at(2).written_rectangle_vec.at(0).x,
              20u);
    EXPECT_EQ(dependencies.version_vec.at(2).written_rectangle_vec.at(0).y,
              5u);
}

TEST(DependencyAnalyzer, AWriteThatDoesNotSayWhereItLandedMakesTheExtentNotKnown)
{
    Scene scene;
    const auto texture = scene.create_resource(1,
                                               ResourceKind::TEXTURE_2D,
                                               true); /* in_is_render_target */

    scene.copy_to(texture, 0, 0, 10, 10);
    scene.end_frame();

    scene.draw_to(texture);
    scene.end_frame();

    const auto result = scene.analyze();

    EXPECT_FALSE(result.resource_vec.at(0).written_extent.is_known);
    EXPECT_TRUE (result.resource_vec.at(0).version_vec.at(1).written_rectangle_vec.empty() );
}

TEST(DependencyAnalyzer, AWriteToAnotherMipLevelMakesTheExtentNotKnown)
{
    Scene scene;
    const auto texture = scene.create_resource(2);

    scene.copy_to(texture, 0, 0, 10, 10);
    scene.copy_to(texture, 0, 0, 5, 5, 1 /* in_mip_level */);
    scene.end_frame();

    EXPECT_FALSE(scene.analyze().resource_vec.at(0).written_extent.is_known);
}

TEST(DependencyAnalyzer, AWriteThatReplacesAllOfTheResourceIsNotOneThatTheExtentIsOf)
{
    Scene scene;
    const auto texture = scene.create_resource();

    scene.access(texture,
                 AccessRole::UPLOAD_DESTINATION,
                 AccessMode::WRITE,
                 AccessCoverage::FULL);
    scene.next_call();
    scene.end_frame();

    scene.copy_to(texture, 4, 4, 8, 8);
    scene.end_frame();

    const auto  result       = scene.analyze();
    const auto& dependencies = result.resource_vec.at(0);

    ASSERT_TRUE(dependencies.written_extent.is_known);

    EXPECT_EQ(dependencies.written_extent.x,
              4u);
    EXPECT_EQ(dependencies.written_extent.width,
              8u);
    /* Nothing reads the texture before the upload, so its version is the first. The frame that replaced all of the texture has nothing to say of where. */
    ASSERT_EQ(dependencies.version_vec.size(),
              2u);
    EXPECT_TRUE(dependencies.version_vec.at(0).written_rectangle_vec.empty() );
    EXPECT_EQ  (dependencies.version_vec.at(1).written_rectangle_vec.size(),
                1u);
}

namespace
{
    /* A frame of the shape of the one that the inputs are about: the target is cleared, a draw reads a texture and renders to it, a copy takes it to the
     * strip of another texture, and a draw samples that texture and renders to the target.
     */
    void make_strip_frame(Scene*            inout_scene_ptr,
                          const ResourceId& in_source,
                          const ResourceId& in_strip_texture,
                          const ResourceId& in_target,
                          const uint32_t&   in_strip_x,
                          const bool&       in_samples_strip_first)
    {
        inout_scene_ptr->clear_fully(in_target);

        if (in_samples_strip_first)
        {
            inout_scene_ptr->access(in_strip_texture,
                                    AccessRole::SAMPLED,
                                    AccessMode::READ);
            inout_scene_ptr->access(in_target,
                                    AccessRole::COLOR_ATTACHMENT,
                                    AccessMode::WRITE);
            inout_scene_ptr->next_call();
        }

        inout_scene_ptr->access(in_source,
                                AccessRole::SAMPLED,
                                AccessMode::READ);
        inout_scene_ptr->access(in_target,
                                AccessRole::COLOR_ATTACHMENT,
                                AccessMode::WRITE);
        inout_scene_ptr->next_call();

        inout_scene_ptr->access(in_target,
                                AccessRole::TRANSFER_SOURCE,
                                AccessMode::READ);
        inout_scene_ptr->copy_to(in_strip_texture,
                                 in_strip_x,
                                 0,
                                 10,
                                 10);

        inout_scene_ptr->access(in_strip_texture,
                                AccessRole::SAMPLED,
                                AccessMode::READ);
        inout_scene_ptr->access(in_target,
                                AccessRole::COLOR_ATTACHMENT,
                                AccessMode::WRITE);
        inout_scene_ptr->next_call();
        inout_scene_ptr->end_frame();
    }
}

TEST(DependencyAnalyzer, WhatFlowsIntoAStripIsWhatTheCallsThatMadeItReadAndNotWhatIsReadAfterIt)
{
    Scene scene;
    const auto source = scene.create_resource(); /* resource 1 */
    const auto strips = scene.create_resource(); /* resource 2 */
    const auto target = scene.create_resource(1,
                                              ResourceKind::TEXTURE_2D,
                                              true); /* resource 3; in_is_render_target */

    for (uint32_t n_frame = 0;
                  n_frame < 3;
                ++n_frame)
    {
        make_strip_frame(&scene,
                         source,
                         strips,
                         target,
                         n_frame * 10,
                         false /* in_samples_strip_first */);
    }

    const auto  result       = scene.analyze();
    const auto& dependencies = result.resource_vec.at(1);

    ASSERT_TRUE(dependencies.written_extent.is_known);
    ASSERT_EQ(dependencies.version_vec.size(),
              4u);

    for (size_t n_version = 1;
                n_version < dependencies.version_vec.size();
              ++n_version)
    {
        const auto& version = dependencies.version_vec.at(n_version);

        /* The strip is what the target was when it was copied, and that is what the draw before the copy made of the texture it read: resource 1, which is not
         * the strip texture itself (the draw that samples it comes after the copy).
         */
        ASSERT_TRUE(version.has_inputs) << "version " << n_version;
        EXPECT_EQ(version.input_resource_vec,
                  (std::vector<uint32_t>{1}) ) << "version " << n_version;
    }
}

TEST(DependencyAnalyzer, WhenTheContentOfAResourceFlowsIntoWhatIsWrittenToItItsVersionsDoNotSayWhatFlowsIntoThem)
{
    Scene scene;
    const auto source = scene.create_resource();
    const auto strips = scene.create_resource();
    const auto target = scene.create_resource(1,
                                              ResourceKind::TEXTURE_2D,
                                              true); /* in_is_render_target */

    /* A draw that samples the strips and renders to the target comes before the copy: what the strips had flows into what is copied to them. */
    make_strip_frame(&scene,
                     source,
                     strips,
                     target,
                     0,
                     true /* in_samples_strip_first */);

    const auto result = scene.analyze();

    ASSERT_TRUE(result.resource_vec.at(1).written_extent.is_known);
    EXPECT_FALSE(result.resource_vec.at(1).version_vec.at(1).has_inputs);
    EXPECT_TRUE (result.resource_vec.at(1).version_vec.at(1).input_resource_vec.empty() );
}

TEST(DependencyAnalyzer, ResourcesThatNoWrittenExtentIsKnownOfHaveNoInputs)
{
    Scene scene;
    const auto texture = scene.create_resource(1,
                                               ResourceKind::TEXTURE_2D,
                                               true); /* in_is_render_target */

    scene.draw_to(texture);
    scene.end_frame();

    for (const auto& version : scene.analyze().resource_vec.at(0).version_vec)
    {
        EXPECT_FALSE(version.has_inputs);
    }
}
