/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Checks that GLPipelineState gives the same description of a draw whether or not an earlier description is remembered: the key and the flags that a draw
 * takes from the state are kept until the state changes, so every way of changing the state has to forget them.
 */
#include <gtest/gtest.h>
#include "analysis/api_dump_analysis/frontends/OpenGL/gl_pipeline_state.h"
#include <functional>
#include <vector>

namespace
{
    using Analysis::APIDumpAnalysis::Frontends::GLPipelineState;
    using Analysis::APIDumpAnalysis::Frontends::GLStateValue;

    constexpr uint32_t BLEND        = 0x0BE2;
    constexpr uint32_t DEPTH_TEST   = 0x0B71;
    constexpr uint32_t STENCIL_TEST = 0x0B90;

    /* One change of the state; the saves are the copies that glPushAttrib made, by number. */
    typedef std::function<void(GLPipelineState*, std::vector<GLPipelineState>*)> ChangeFunc;

    uint32_t next_random(uint32_t* inout_seed_ptr)
    {
        *inout_seed_ptr = *inout_seed_ptr * 1664525u + 1013904223u;

        return *inout_seed_ptr >> 8;
    }

    ChangeFunc make_change(uint32_t* inout_seed_ptr)
    {
        constexpr uint32_t CAPABILITIES[] = {BLEND, DEPTH_TEST, STENCIL_TEST, 0x0B44, 0x0B50, 0x0DE1, 0x0C60};
        constexpr uint32_t MASKS[]        = {0x00002000, 0x00004000, 0x00000100, 0x00000008, 0xFFFFFFFF};

        const uint32_t kind       = next_random(inout_seed_ptr) % 5;
        const uint32_t capability = CAPABILITIES[next_random(inout_seed_ptr) % 7];
        const bool     is_enabled = (next_random(inout_seed_ptr) % 2 == 0);
        const uint32_t unit       = next_random(inout_seed_ptr) % 3;
        const auto     value      = static_cast<GLStateValue>(next_random(inout_seed_ptr) % 18);
        const uint32_t setting    = next_random(inout_seed_ptr) % 4;
        const uint32_t mask       = MASKS[next_random(inout_seed_ptr) % 5];
        const uint32_t save       = next_random(inout_seed_ptr);

        ChangeFunc result;

        switch (kind)
        {
            case 0:
            {
                result = [=](GLPipelineState* inout_state_ptr, std::vector<GLPipelineState>*)
                {
                    inout_state_ptr->set_capability(capability,
                                                    is_enabled);
                };

                break;
            }

            case 1:
            {
                result = [=](GLPipelineState* inout_state_ptr, std::vector<GLPipelineState>*)
                {
                    inout_state_ptr->set_unit_capability(unit,
                                                         capability,
                                                         is_enabled);
                };

                break;
            }

            case 2:
            {
                result = [=](GLPipelineState* inout_state_ptr, std::vector<GLPipelineState>*)
                {
                    inout_state_ptr->set_value(value,
                                               setting);
                };

                break;
            }

            case 3:
            {
                result = [=](GLPipelineState* inout_state_ptr, std::vector<GLPipelineState>* inout_save_vec_ptr)
                {
                    inout_save_vec_ptr->push_back(*inout_state_ptr);
                };

                break;
            }

            default:
            {
                result = [=](GLPipelineState* inout_state_ptr, std::vector<GLPipelineState>* inout_save_vec_ptr)
                {
                    if (inout_save_vec_ptr->empty() == false)
                    {
                        inout_state_ptr->restore(inout_save_vec_ptr->at(save % inout_save_vec_ptr->size() ),
                                                 mask);
                    }
                };

                break;
            }
        }

        return result;
    }
}

TEST(GLPipelineStateSummary, AnEarlierDescriptionNeverChangesALaterOne)
{
    constexpr uint32_t N_CHANGES = 400;

    GLPipelineState                 remembering_state;
    std::vector<GLPipelineState>    remembering_save_vec;
    std::vector<ChangeFunc>         change_vec;
    uint32_t                        seed = 99;

    for (uint32_t n_change = 0;
                  n_change < N_CHANGES;
                ++n_change)
    {
        change_vec.push_back(make_change(&seed) );

        change_vec.back()(&remembering_state,
                          &remembering_save_vec);

        /* A state that is described after every change remembers its last description. The other has the same changes made and is described only at the end. */
        const auto remembered = remembering_state.describe_draw(Analysis::APIDumpAnalysis::PrimitiveTopology::TRIANGLE_LIST,
                                                                3);

        GLPipelineState              fresh_state;
        std::vector<GLPipelineState> fresh_save_vec;

        for (const auto& change_func : change_vec)
        {
            change_func(&fresh_state,
                        &fresh_save_vec);
        }

        const auto fresh = fresh_state.describe_draw(Analysis::APIDumpAnalysis::PrimitiveTopology::TRIANGLE_LIST,
                                                     3);

        EXPECT_EQ(remembered.state_key,
                  fresh.state_key)                << "after change " << n_change;
        EXPECT_EQ(remembered.reads_color_attachments,
                  fresh.reads_color_attachments)  << "after change " << n_change;
        EXPECT_EQ(remembered.reads_depth_stencil,
                  fresh.reads_depth_stencil)      << "after change " << n_change;
    }
}

TEST(GLPipelineStateSummary, ACopyDescribesTheSameAsTheOriginalAndThenFollowsItsOwnChanges)
{
    GLPipelineState original;

    original.set_capability(BLEND,
                            true);

    const auto before = original.describe_draw(Analysis::APIDumpAnalysis::PrimitiveTopology::TRIANGLE_LIST,
                                               3);

    GLPipelineState copy = original;

    EXPECT_EQ(copy.describe_draw(Analysis::APIDumpAnalysis::PrimitiveTopology::TRIANGLE_LIST,
                                 3).state_key,
              before.state_key);

    copy.set_capability(DEPTH_TEST,
                        true);

    EXPECT_NE(copy.describe_draw(Analysis::APIDumpAnalysis::PrimitiveTopology::TRIANGLE_LIST,
                                 3).state_key,
              before.state_key);
    EXPECT_EQ(original.describe_draw(Analysis::APIDumpAnalysis::PrimitiveTopology::TRIANGLE_LIST,
                                     3).state_key,
              before.state_key);
}

TEST(GLPipelineStateSummary, SettingWhatIsAlreadySetKeepsTheKeyAndSettingSomethingElseChangesIt)
{
    GLPipelineState state;

    state.set_capability(BLEND,
                         true);
    state.set_value(GLStateValue::BLEND_SOURCE,
                    1);

    const auto first = state.describe_draw(Analysis::APIDumpAnalysis::PrimitiveTopology::TRIANGLE_LIST,
                                           3);

    /* The same calls again, and the disabling of something that was never enabled, change nothing. */
    state.set_capability(BLEND,
                         true);
    state.set_capability(DEPTH_TEST,
                         false);
    state.set_value(GLStateValue::BLEND_SOURCE,
                    1);

    EXPECT_EQ(state.describe_draw(Analysis::APIDumpAnalysis::PrimitiveTopology::TRIANGLE_LIST,
                                  3).state_key,
              first.state_key);

    state.set_value(GLStateValue::BLEND_SOURCE,
                    2);

    EXPECT_NE(state.describe_draw(Analysis::APIDumpAnalysis::PrimitiveTopology::TRIANGLE_LIST,
                                  3).state_key,
              first.state_key);

    state.set_value(GLStateValue::BLEND_SOURCE,
                    1);

    EXPECT_EQ(state.describe_draw(Analysis::APIDumpAnalysis::PrimitiveTopology::TRIANGLE_LIST,
                                  3).state_key,
              first.state_key);

    state.set_capability(BLEND,
                         false);

    EXPECT_FALSE(state.describe_draw(Analysis::APIDumpAnalysis::PrimitiveTopology::TRIANGLE_LIST,
                                     3).reads_color_attachments);
}
