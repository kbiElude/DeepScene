/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Tests for FrameRepeatability: which frames can be played again and again from the state that the frame before leaves.
 */
#include <gtest/gtest.h>
#include "dependency_scene.h"
#include "tools/stereo_replay/consistency_checker/frame_repeatability.h"

namespace
{
    using Stereo::Replay::ConsistencyChecker::FrameRepeatability;

    bool is_repeatable(const Tests::DependencyScene& in_scene,
                       const uint32_t&               in_frame_index,
                       std::string*                  out_reason_ptr)
    {
        return FrameRepeatability::is_repeatable(in_scene.make_analysis(),
                                                 in_frame_index,
                                                 out_reason_ptr);
    }
}

TEST(FrameRepeatability, AFrameThatReplacesWhatItWritesBeforeItIsReadCanBePlayedAgain)
{
    Tests::DependencyScene scene;
    std::string            reason;
    const auto             target = scene.create_resource(1,
                                                          Analysis::APIDumpAnalysis::ResourceKind::TEXTURE_2D,
                                                          true); /* in_is_render_target */

    scene.end_frame();

    /* Frame 1 clears the target and draws to it; frame 2 samples it. */
    scene.clear_fully(target);
    scene.draw_to    (target);
    scene.end_frame  ();

    scene.sample   (target);
    scene.end_frame();

    EXPECT_TRUE(is_repeatable(scene,
                              1,
                             &reason) ) << reason;
}

TEST(FrameRepeatability, AFrameThatDrawsOverWhatIsThereCannotBePlayedAgain)
{
    Tests::DependencyScene scene;
    std::string            reason;
    const auto             target = scene.create_resource(1,
                                                          Analysis::APIDumpAnalysis::ResourceKind::TEXTURE_2D,
                                                          true); /* in_is_render_target */

    /* Each frame reads the target (what the frame before left in it) and then draws to it. */
    scene.end_frame();

    scene.sample   (target);
    scene.draw_to  (target);
    scene.end_frame();

    scene.sample   (target);
    scene.draw_to  (target);
    scene.end_frame();

    EXPECT_FALSE(is_repeatable(scene,
                               1,
                              &reason) );
    EXPECT_FALSE(reason.empty() );
}

TEST(FrameRepeatability, AFrameTheDependenciesDoNotKnowCannotBePlayedAgain)
{
    Tests::DependencyScene scene;
    std::string            reason;

    scene.end_frame();

    EXPECT_FALSE(is_repeatable(scene,
                               5,
                              &reason) );
    EXPECT_FALSE(reason.empty() );
}

TEST(FrameRepeatability, AFrameThatMakesAResourceCannotBePlayedAgain)
{
    Tests::DependencyScene scene;
    std::string            reason;

    scene.end_frame();
    scene.end_frame();

    /* The resource is made by a call of frame 2. */
    const auto resource = scene.create_resource();

    scene.next_call  ();
    scene.clear_fully(resource);
    scene.end_frame  ();

    auto analysis = scene.make_analysis();

    analysis.resource_vec.at(0).created_at_call_index = analysis.frame_call_range_vec.at(2).first_call_index;

    EXPECT_FALSE(FrameRepeatability::is_repeatable(analysis,
                                                   2,
                                                  &reason) );
    EXPECT_FALSE(reason.empty() );
}
