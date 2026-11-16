/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Tests for what the adapters of the APIs share in making an application render larger than it asks: the scale, the lines of the log, and the reasons that an application
 * is stopped for.
 */
#include <gtest/gtest.h>
#include "analysis/api_dump_analysis/types.h"
#include "eye_image_limits.h"
#include "render_scale_planner.h"
#include "resolution_hazard_gate.h"
#include <string>
#include <vector>

TEST(RenderScalePlannerTest, TheScaleIsTheLargestThatFitsTheEyeWithTheProportionsOfTheImage)
{
    /* 2064 / 640 = 3.225, and the height (2208 / 480) allows more. */
    EXPECT_NEAR(Stereo::ConverterDLL::RenderScalePlanner::choose_scale(2064,
                                                                       2208,
                                                                       640,
                                                                       480,
                                                                       0),
                3.225f,
                0.001f);
}

TEST(RenderScalePlannerTest, TheScaleIsNotMoreThanASurfaceOfTheApiCanBe)
{
    /* The surface can be 2048 on a side: for an image of 800 x 600 that is 2048 / 800 = 2.56. */
    EXPECT_NEAR(Stereo::ConverterDLL::RenderScalePlanner::choose_scale(4000,
                                                                       4000,
                                                                       800,
                                                                       600,
                                                                       2048),
                2.56f,
                0.001f);
}

TEST(RenderScalePlannerTest, TheScaleIsNotMoreThanTheChannelHoldsForTwoEyes)
{
    const float scale = Stereo::ConverterDLL::RenderScalePlanner::choose_scale(100000,
                                                                               100000,
                                                                               640,
                                                                               480,
                                                                               0);

    EXPECT_NEAR(scale,
                Stereo::ConverterDLL::EyeImageLimits::get_largest_scale(640,
                                                                        480),
                0.001f);
}

TEST(RenderScalePlannerTest, NoTargetLeavesTheScaleAtOne)
{
    EXPECT_FLOAT_EQ(Stereo::ConverterDLL::RenderScalePlanner::choose_scale(0,
                                                                           0,
                                                                           640,
                                                                           480,
                                                                           0),
                    1.0f);
}

TEST(RenderScalePlannerTest, TheLinesOfTheLogSayWhatWasMadeLargerAndWhatCannotBePassedOn)
{
    const std::string resize = Stereo::ConverterDLL::RenderScalePlanner::describe_resize("framebuffer",
                                                                                         640,
                                                                                         480,
                                                                                         2064,
                                                                                         1548,
                                                                                         3.225f);

    EXPECT_NE(resize.find("The framebuffer is made 2064 x 1548 instead of 640 x 480"),
              std::string::npos) << resize;
    EXPECT_NE(resize.find("scale 3.2"),
              std::string::npos) << resize;
    EXPECT_NE(Stereo::ConverterDLL::RenderScalePlanner::describe_too_large(5000,
                                                                           4000,
                                                                           4.0f).find("lower the render scale"),
              std::string::npos);
}

TEST(ResolutionHazardGateTest, EveryKindOfHazardHasAReasonOfItsOwn)
{
    using Analysis::APIDumpAnalysis::ResolutionHazard;

    std::vector<std::string> reason_vec;

    for (const auto hazard : {ResolutionHazard::CLEARS_WITH_RECTANGLES, ResolutionHazard::COPIES_WITH_RECTANGLES, ResolutionHazard::LINE_WIDTH_STATES,
                              ResolutionHazard::PIXEL_READBACKS,        ResolutionHazard::POINT_SIZE_STATES,      ResolutionHazard::SCISSOR_BOXES,
                              ResolutionHazard::SCREEN_SPACE_VERTICES,  ResolutionHazard::SHADER_CREATIONS,       ResolutionHazard::SIZE_QUERIES,
                              ResolutionHazard::RASTER_OPERATIONS,      ResolutionHazard::TEXTURE_UPLOADS_TO_RENDER_TARGETS})
    {
        reason_vec.push_back(Stereo::ConverterDLL::ResolutionHazardGate::get_reason(hazard) );
    }

    for (size_t n_reason = 0;
                n_reason < reason_vec.size();
              ++n_reason)
    {
        for (size_t n_other = n_reason + 1;
                    n_other < reason_vec.size();
                  ++n_other)
        {
            EXPECT_NE(reason_vec.at(n_reason),
                      reason_vec.at(n_other) );
        }
    }
}

TEST(ResolutionHazardGateTest, NothingStopsAnApplicationThatIsNotRenderedLarger)
{
    bool                     has_failed = false;
    std::string              message;
    Analysis::Recipe::Recipe recipe;

    recipe.is_baked = true;

    Stereo::Core::RecipeStereoPlanner core(recipe,
                                           [&](const std::string& in_message)
                                           {
                                               has_failed = true;
                                               message    = in_message;
                                           });

    Stereo::ConverterDLL::ResolutionHazardGate::require_unaffected_by_scale(&core,
                                                                            Stereo::Core::ResolutionPolicy(1.0f),
                                                                            Analysis::APIDumpAnalysis::ResolutionHazard::SCISSOR_BOXES);

    EXPECT_FALSE(has_failed);

    Stereo::ConverterDLL::ResolutionHazardGate::require_unaffected_by_scale(&core,
                                                                            Stereo::Core::ResolutionPolicy(2.0f),
                                                                            Analysis::APIDumpAnalysis::ResolutionHazard::SCISSOR_BOXES);

    EXPECT_TRUE(has_failed);
    EXPECT_NE(message.find("scissor box"),
              std::string::npos) << message;
    EXPECT_NE(message.find("rendered larger than it asks"),
              std::string::npos) << message;
}

TEST(RenderScalePlannerTest, ThePictureIsCenteredInATargetOfOtherProportions)
{
    /* The picture fits: the frame is the target, with bars at the sides. */
    const Stereo::ConverterDLL::EyeLayout layout = Stereo::ConverterDLL::RenderScalePlanner::choose_layout(2064,
                                                                                                           1161,
                                                                                                           1548,
                                                                                                           1161,
                                                                                                           0);

    EXPECT_EQ(layout.frame_width,
              2064u);
    EXPECT_EQ(layout.frame_height,
              1161u);
    EXPECT_EQ(layout.picture.width,
              1548u);
    EXPECT_EQ(layout.picture.height,
              1161u);
    EXPECT_EQ(layout.picture.x,
              258u);
    EXPECT_EQ(layout.picture.y,
              0u);
}

TEST(RenderScalePlannerTest, WithoutATargetTheFrameIsThePicture)
{
    const Stereo::ConverterDLL::EyeLayout layout = Stereo::ConverterDLL::RenderScalePlanner::choose_layout(0,
                                                                                                           0,
                                                                                                           640,
                                                                                                           480,
                                                                                                           0);

    EXPECT_EQ(layout.frame_width,
              640u);
    EXPECT_EQ(layout.frame_height,
              480u);
    EXPECT_EQ(layout.picture.x,
              0u);
    EXPECT_EQ(layout.picture.y,
              0u);
}

TEST(RenderScalePlannerTest, APictureLargerThanTheTargetOrATargetBeyondTheLimitMakesTheFrameThePicture)
{
    /* Not smaller than asked. */
    const Stereo::ConverterDLL::EyeLayout larger = Stereo::ConverterDLL::RenderScalePlanner::choose_layout(800,
                                                                                                           600,
                                                                                                           1280,
                                                                                                           720,
                                                                                                           0);

    EXPECT_EQ(larger.frame_width,
              1280u);
    EXPECT_EQ(larger.frame_height,
              720u);

    /* The API cannot make surfaces of 4096. */
    const Stereo::ConverterDLL::EyeLayout beyond = Stereo::ConverterDLL::RenderScalePlanner::choose_layout(4096,
                                                                                                           2048,
                                                                                                           2048,
                                                                                                           1152,
                                                                                                           2048);

    EXPECT_EQ(beyond.frame_width,
              2048u);
    EXPECT_EQ(beyond.frame_height,
              1152u);
}
