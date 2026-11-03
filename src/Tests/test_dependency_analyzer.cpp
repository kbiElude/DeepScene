/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Checks of what the analysis of the dependencies tells of accesses made up by hand: what a frame uses of what the frames before it left in a
 * resource, and which frame left it.
 */
#include <gtest/gtest.h>
#include "Analysis/APIDumpDependencies/dependency_analyzer.h"
#include <vector>

namespace
{
    using namespace Analysis::APIDumpAnalysis;
    using namespace Analysis::APIDumpDependencies;

    /* Accesses are made one call at a time (all of those of a call share its number), and frames end where they are told to. */
    class Scene
    {
    public:
        void access(const ResourceId&       in_id,
                    const AccessRole&       in_role,
                    const AccessMode&       in_mode,
                    const AccessCoverage&   in_coverage      = AccessCoverage::UNKNOWN,
                    const SubresourceRange& in_range         = SubresourceRange() )
        {
            AccessEvent event;

            event.call_index        = m_call_index;
            event.coverage          = in_coverage;
            event.mode              = in_mode;
            event.resource_id       = in_id;
            event.role              = in_role;
            event.subresource_range = in_range;

            m_log.on_access(event);
        }

        DependencyResult analyze(const uint32_t& in_n_threads = 1) const
        {
            return DependencyAnalyzer::analyze(m_log,
                                               m_resource_vec,
                                               in_n_threads);
        }

        void end_frame()
        {
            m_log.on_frame_ended(m_n_frames_ended++,
                                 m_first_call_index,
                                 m_call_index);

            m_call_index++;

            m_first_call_index = m_call_index;
        }

        /* The next call: what is accessed from now on is accessed by another call. */
        void next_call()
        {
            m_call_index++;
        }

        ResourceId create_resource(const uint32_t& in_n_mip_levels = 1)
        {
            Resource resource;

            resource.description.height          = 64;
            resource.description.mip_level_count = in_n_mip_levels;
            resource.description.width           = 64;
            resource.id                          = make_resource_id(static_cast<uint32_t>(m_resource_vec.size() ) + 1);

            m_resource_vec.push_back(resource);

            return resource.id;
        }

        /* A clear of all of the resource. */
        void clear_fully(const ResourceId& in_id)
        {
            access(in_id,
                   AccessRole::CLEAR_TARGET,
                   AccessMode::WRITE,
                   AccessCoverage::FULL);
            next_call();
        }

        /* A draw which writes where it reaches, which is not known. */
        void draw_to(const ResourceId& in_id)
        {
            access(in_id,
                   AccessRole::COLOR_ATTACHMENT,
                   AccessMode::WRITE);
            next_call();
        }

        void sample(const ResourceId& in_id)
        {
            access(in_id,
                   AccessRole::SAMPLED,
                   AccessMode::READ);
            next_call();
        }

    private:
        uint64_t               m_call_index        = 0;
        uint64_t               m_first_call_index  = 0;
        AccessLog              m_log;
        uint64_t               m_n_frames_ended    = 0;
        std::vector<Resource>  m_resource_vec;
    };
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
