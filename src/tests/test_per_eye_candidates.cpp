/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Checks of which resources each eye has a copy of, and of the rotation groups: resources of one description which the frames write by turns.
 */
#include <gtest/gtest.h>
#include "analysis/api_dump_dependencies/per_eye_candidates.h"
#include "dependency_scene.h"

namespace
{
    using namespace Analysis::APIDumpAnalysis;
    using namespace Analysis::APIDumpDependencies;

    constexpr uint64_t SAME_DESCRIPTION = 0xAA;

    /* The frames render to the target of their turn, and read the one that the frame before rendered to. */
    void make_frames(Tests::DependencyScene*     inout_scene_ptr,
                     const std::vector<ResourceId>& in_turn_vec,
                     const size_t&               in_n_frames)
    {
        for (size_t n_frame = 0;
                    n_frame < in_n_frames;
                  ++n_frame)
        {
            const ResourceId& target_id   = in_turn_vec.at(n_frame % in_turn_vec.size() );
            const ResourceId& previous_id = in_turn_vec.at( (n_frame + in_turn_vec.size() - 1) % in_turn_vec.size() );

            inout_scene_ptr->sample    (previous_id);
            inout_scene_ptr->clear_fully(target_id);
            inout_scene_ptr->draw_to   (target_id);
            inout_scene_ptr->end_frame ();
        }
    }
}

TEST(PerEyeCandidates, TwoTargetsWrittenByTurnsAreAGroupWithARegularOrder)
{
    Tests::DependencyScene scene;
    const auto             first  = scene.create_resource(1, ResourceKind::TEXTURE_2D, true, SAME_DESCRIPTION);
    const auto             second = scene.create_resource(1, ResourceKind::TEXTURE_2D, true, SAME_DESCRIPTION);

    make_frames(&scene,
                {first, second},
                8);

    const auto analysis  = scene.make_analysis();
    const auto group_vec = PerEyeCandidates::find_rotation_groups(analysis);

    ASSERT_EQ(group_vec.size(),
              1u);
    EXPECT_EQ(group_vec.at(0).n_resource_vec,
              (std::vector<size_t>{0, 1}) );
    EXPECT_TRUE(group_vec.at(0).has_regular_order);
}

TEST(PerEyeCandidates, ThreeTargetsWhichTheFramesTakeInNoRepeatingOrderAreAGroupWithAnIrregularOrder)
{
    Tests::DependencyScene scene;
    const auto             first  = scene.create_resource(1, ResourceKind::TEXTURE_2D, true, SAME_DESCRIPTION);
    const auto             second = scene.create_resource(1, ResourceKind::TEXTURE_2D, true, SAME_DESCRIPTION);
    const auto             third  = scene.create_resource(1, ResourceKind::TEXTURE_2D, true, SAME_DESCRIPTION);

    /* A swap chain of three images which the API hands out in the order it likes. */
    make_frames(&scene,
                {first, second, third, second, first, third, first, second, third, second, first, third},
                12);

    const auto group_vec = PerEyeCandidates::find_rotation_groups(scene.make_analysis() );

    ASSERT_EQ(group_vec.size(),
              1u);
    EXPECT_EQ(group_vec.at(0).n_resource_vec.size(),
              3u);
    EXPECT_FALSE(group_vec.at(0).has_regular_order);
}

TEST(PerEyeCandidates, TargetsOfOtherDescriptionsAreNotAGroup)
{
    Tests::DependencyScene scene;
    const auto             first  = scene.create_resource(1, ResourceKind::TEXTURE_2D, true, SAME_DESCRIPTION);
    const auto             second = scene.create_resource(1, ResourceKind::TEXTURE_2D, true, SAME_DESCRIPTION + 1);

    make_frames(&scene,
                {first, second},
                8);

    EXPECT_TRUE(PerEyeCandidates::find_rotation_groups(scene.make_analysis() ).empty() );
}

TEST(PerEyeCandidates, ATargetWhichIsWrittenInRunsIsNotAGroup)
{
    Tests::DependencyScene scene;
    const auto             first  = scene.create_resource(1, ResourceKind::TEXTURE_2D, true, SAME_DESCRIPTION);
    const auto             second = scene.create_resource(1, ResourceKind::TEXTURE_2D, true, SAME_DESCRIPTION);

    /* Four frames of one, then four of the other: no turns. */
    make_frames(&scene,
                {first, first},
                4);
    make_frames(&scene,
                {second, second},
                4);

    EXPECT_TRUE(PerEyeCandidates::find_rotation_groups(scene.make_analysis() ).empty() );
}

TEST(PerEyeCandidates, EveryTargetOfAGroupIsACandidateIfOneIs)
{
    Tests::DependencyScene scene;
    const auto             first  = scene.create_resource(1, ResourceKind::TEXTURE_2D, true, SAME_DESCRIPTION);
    const auto             second = scene.create_resource(1, ResourceKind::TEXTURE_2D, true, SAME_DESCRIPTION);
    const auto             third  = scene.create_resource(1, ResourceKind::TEXTURE_2D, true, SAME_DESCRIPTION);

    /* The first two carry content; the third takes its turns too, but each frame clears it and nobody reads it. */
    make_frames(&scene,
                {first, second},
                6);

    for (size_t n_frame = 0;
                n_frame < 4;
              ++n_frame)
    {
        scene.clear_fully(third);
        scene.draw_to   (third);
        scene.end_frame ();

        scene.sample    (first);
        scene.clear_fully(first);
        scene.draw_to   (first);
        scene.end_frame ();

        scene.sample    (second);
        scene.clear_fully(second);
        scene.draw_to   (second);
        scene.end_frame ();
    }

    const auto analysis      = scene.make_analysis();
    const auto candidate_vec = PerEyeCandidates::find(analysis);
    size_t     n_added       = 0;

    for (const auto& candidate : candidate_vec)
    {
        n_added += (candidate.is_added_by_group) ? 1 : 0;
    }

    EXPECT_EQ(candidate_vec.size(),
              3u);
    EXPECT_EQ(n_added,
              1u);
}
