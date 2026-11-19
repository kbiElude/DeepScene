/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * What the application renders to, as the model builder is told of it: the tracker says whether the color target is a new one, which is when a viewport that covers all
 * of it begins, and it keeps the views for the front ends that read them back.
 */
#include <gtest/gtest.h>
#include "analysis/api_dump_analysis/render_target_tracker.h"

namespace
{
    Analysis::APIDumpAnalysis::ResourceView make_view(const uint32_t& in_resource)
    {
        Analysis::APIDumpAnalysis::ResourceView result;

        result.resource_id = static_cast<Analysis::APIDumpAnalysis::ResourceId>(in_resource);

        return result;
    }
}

TEST(RenderTargetTracker, ATrackerThatWasNeverSetHasNoViews)
{
    Analysis::APIDumpAnalysis::ModelBuilder        builder(false); /* in_keeps_history */
    Analysis::APIDumpAnalysis::RenderTargetTracker tracker(&builder);

    EXPECT_EQ(tracker.get_color_view().resource_id,
              Analysis::APIDumpAnalysis::ResourceId::NONE);
    EXPECT_EQ(tracker.get_depth_stencil_view().resource_id,
              Analysis::APIDumpAnalysis::ResourceId::NONE);
}

TEST(RenderTargetTracker, ANewColorTargetIsToldAndTheSameOneAgainIsNot)
{
    Analysis::APIDumpAnalysis::ModelBuilder        builder(false); /* in_keeps_history */
    Analysis::APIDumpAnalysis::RenderTargetTracker tracker(&builder);

    EXPECT_TRUE(tracker.set(make_view(3),
                            make_view(4),
                            false) ); /* in_tells_unchanged */
    EXPECT_FALSE(tracker.set(make_view(3),
                             make_view(4),
                             false) ); /* in_tells_unchanged */
    EXPECT_FALSE(tracker.set(make_view(3),
                             make_view(5),
                             false) ); /* in_tells_unchanged */
    EXPECT_TRUE(tracker.set(make_view(6),
                            make_view(5),
                            false) ); /* in_tells_unchanged */
}

TEST(RenderTargetTracker, TheViewsAreKeptAsTheyWereSet)
{
    Analysis::APIDumpAnalysis::ModelBuilder        builder(false); /* in_keeps_history */
    Analysis::APIDumpAnalysis::RenderTargetTracker tracker(&builder);

    tracker.set(make_view(7),
                make_view(8),
                true); /* in_tells_unchanged */

    EXPECT_EQ(tracker.get_color_view().resource_id,
              make_view(7).resource_id);
    EXPECT_EQ(tracker.get_depth_stencil_view().resource_id,
              make_view(8).resource_id);
}

TEST(RenderTargetTracker, AColorViewThatWasForgottenIsANewTargetWhenItIsSetAgain)
{
    Analysis::APIDumpAnalysis::ModelBuilder        builder(false); /* in_keeps_history */
    Analysis::APIDumpAnalysis::RenderTargetTracker tracker(&builder);

    tracker.set(make_view(2),
                make_view(0),
                false); /* in_tells_unchanged */

    tracker.forget_color_view();

    EXPECT_EQ(tracker.get_color_view().resource_id,
              Analysis::APIDumpAnalysis::ResourceId::NONE);
    EXPECT_TRUE(tracker.set(make_view(2),
                            make_view(0),
                            false) ); /* in_tells_unchanged */
}

TEST(RenderTargetTracker, ASetWithTheSameViewsMayStillTellTheBuilder)
{
    Analysis::APIDumpAnalysis::ModelBuilder        builder(false); /* in_keeps_history */
    Analysis::APIDumpAnalysis::RenderTargetTracker tracker(&builder);

    tracker.set(make_view(2),
                make_view(3),
                false); /* in_tells_unchanged */

    /* What is told is the model builder's own matter (it ends the open pass); what the tracker answers does not depend on it. */
    EXPECT_FALSE(tracker.set(make_view(2),
                             make_view(3),
                             true) ); /* in_tells_unchanged */
}
