/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Tests for StateCensus: what the calls of the dead frames do to the state, as the calls come.
 */
#include <gtest/gtest.h>
#include "tools/stereo_replay/consistency_checker/state_census.h"

namespace
{
    constexpr uint32_t CAPABILITY_A = 0x0B50;
    constexpr uint32_t TEXTURE0     = 0x84C0;
    constexpr uint32_t TEXTURE1     = 0x84C1;

    Capture::APIDumpLoader::DumpedAPICall make_call(const APIInterceptor::APIFunction& in_function,
                                                    const std::vector<uint32_t>&       in_number_vec = {})
    {
        Capture::APIDumpLoader::DumpedAPICall result;

        result.func = in_function;

        for (const auto number : in_number_vec)
        {
            result.arg_in_vec.push_back(APIInterceptor::APIFunctionArgument::create_u32(number) );
        }

        return result;
    }

    void replay(Stereo::Replay::ConsistencyChecker::StateCensus* inout_census_ptr,
                const Capture::APIDumpLoader::DumpedAPICall&     in_call,
                const uint32_t&                                  in_n_frame,
                const bool&                                      in_was_executed = true)
    {
        inout_census_ptr->on_call_replayed(in_call,
                                           in_n_frame,
                                           in_was_executed);
    }

    const Stereo::Replay::ConsistencyChecker::StateCensus::FunctionEntry* find_entry(const std::vector<Stereo::Replay::ConsistencyChecker::StateCensus::FunctionEntry>& in_entry_vec,
                                                                                       const APIInterceptor::APIFunction&                                                  in_function)
    {
        const Stereo::Replay::ConsistencyChecker::StateCensus::FunctionEntry* result_ptr = nullptr;

        for (const auto& entry : in_entry_vec)
        {
            if (entry.function == in_function)
            {
                result_ptr = &entry;
            }
        }

        return result_ptr;
    }
}

TEST(StateCensus, ACallThatIsOverwrittenInTheSameRunOrGivesTheValueThatTheStateHasIsCounted)
{
    Stereo::Replay::ConsistencyChecker::StateCensus census({false}); /* frame 0 is dead */

    replay(&census, make_call(APIInterceptor::APIFUNCTION_GL_GLENABLE,  {CAPABILITY_A}), 0);
    replay(&census, make_call(APIInterceptor::APIFUNCTION_GL_GLENABLE,  {CAPABILITY_A}), 0);
    replay(&census, make_call(APIInterceptor::APIFUNCTION_GL_GLDISABLE, {CAPABILITY_A}), 0);
    census.finish();

    const auto entry_vec     = census.get_entries();
    const auto* const enable = find_entry(entry_vec, APIInterceptor::APIFUNCTION_GL_GLENABLE);
    const auto* const disable = find_entry(entry_vec, APIInterceptor::APIFUNCTION_GL_GLDISABLE);

    ASSERT_NE(enable,  nullptr);
    ASSERT_NE(disable, nullptr);

    EXPECT_EQ(enable->n_keyed,
              2u);
    EXPECT_EQ(enable->n_repeats,
              1u);
    EXPECT_EQ(enable->n_overwritten_later,
              2u); /* the two are overwritten by the one that disables it */
    EXPECT_EQ(disable->n_overwritten_later,
              0u);
    EXPECT_EQ(census.get_n_runs(),
              1u);
    EXPECT_EQ(census.get_n_changed_keys(),
              1u);
}

TEST(StateCensus, AFrameThatIsMadeEndsTheRunAndWhatItLeavesIsWhatTheNextOneRepeats)
{
    Stereo::Replay::ConsistencyChecker::StateCensus census({false, true, false}); /* the frame in the middle is live */

    replay(&census, make_call(APIInterceptor::APIFUNCTION_GL_GLENABLE, {CAPABILITY_A}), 0);
    replay(&census, make_call(APIInterceptor::APIFUNCTION_GL_GLENABLE, {CAPABILITY_A}), 1);
    replay(&census, make_call(APIInterceptor::APIFUNCTION_GL_GLENABLE, {CAPABILITY_A}), 2);
    census.finish();

    const auto  entry_vec = census.get_entries();
    const auto* const enable = find_entry(entry_vec, APIInterceptor::APIFUNCTION_GL_GLENABLE);

    ASSERT_NE(enable, nullptr);

    EXPECT_EQ(enable->n_calls,
              2u); /* the calls of the frames that are not made */
    EXPECT_EQ(enable->n_overwritten_later,
              0u); /* they are not in one run */
    EXPECT_EQ(enable->n_repeats,
              1u); /* the second gives the value that the frame in the middle left */
    EXPECT_EQ(census.get_n_runs(),
              2u);
}

TEST(StateCensus, ABarrierEndsTheRunAndWhatIsKnownOfTheState)
{
    Stereo::Replay::ConsistencyChecker::StateCensus census({false});

    replay(&census, make_call(APIInterceptor::APIFUNCTION_GL_GLENABLE,     {CAPABILITY_A}), 0);
    replay(&census, make_call(APIInterceptor::APIFUNCTION_GL_GLPUSHATTRIB, {0xFFFFF}), 0);
    replay(&census, make_call(APIInterceptor::APIFUNCTION_GL_GLENABLE,     {CAPABILITY_A}), 0);
    census.finish();

    const auto  entry_vec = census.get_entries();
    const auto* const enable = find_entry(entry_vec, APIInterceptor::APIFUNCTION_GL_GLENABLE);

    ASSERT_NE(enable, nullptr);

    EXPECT_EQ(enable->n_overwritten_later,
              0u);
    EXPECT_EQ(enable->n_repeats,
              0u); /* what the state is is not known after the barrier */
    EXPECT_EQ(census.get_barrier_count_map().at("glPushAttrib"),
              1u);
}

TEST(StateCensus, WhatIsMadeIntoAListIsKeptInItAndChangesNothing)
{
    Stereo::Replay::ConsistencyChecker::StateCensus census({false});

    replay(&census, make_call(APIInterceptor::APIFUNCTION_GL_GLNEWLIST,    {1, 0x1300}), 0);
    replay(&census, make_call(APIInterceptor::APIFUNCTION_GL_GLENABLE,     {CAPABILITY_A}), 0);
    replay(&census, make_call(APIInterceptor::APIFUNCTION_GL_GLENDLIST),                    0);
    replay(&census, make_call(APIInterceptor::APIFUNCTION_GL_GLENABLE,     {CAPABILITY_A}), 0);
    census.finish();

    const auto  entry_vec = census.get_entries();
    const auto* const enable = find_entry(entry_vec, APIInterceptor::APIFUNCTION_GL_GLENABLE);

    ASSERT_NE(enable, nullptr);

    EXPECT_EQ(enable->n_in_lists,
              1u);
    EXPECT_EQ(enable->n_keyed,
              1u);
    EXPECT_EQ(enable->n_repeats,
              0u);
}

TEST(StateCensus, ThePieceOfStateOfATextureEnvironmentIsOfTheActiveUnit)
{
    Stereo::Replay::ConsistencyChecker::StateCensus census({false});

    const auto texenv = make_call(APIInterceptor::APIFUNCTION_GL_GLTEXENVI,
                                  {0x2300, 0x2200, 0x2100});

    replay(&census, make_call(APIInterceptor::APIFUNCTION_GL_GLACTIVETEXTURE, {TEXTURE0}), 0);
    replay(&census, texenv, 0);
    replay(&census, make_call(APIInterceptor::APIFUNCTION_GL_GLACTIVETEXTURE, {TEXTURE1}), 0);
    replay(&census, texenv, 0);
    census.finish();

    const auto  entry_vec = census.get_entries();
    const auto* const texenv_entry = find_entry(entry_vec, APIInterceptor::APIFUNCTION_GL_GLTEXENVI);

    ASSERT_NE(texenv_entry, nullptr);

    EXPECT_EQ(texenv_entry->n_overwritten_later,
              0u); /* the other unit has a texture environment of its own */
    EXPECT_EQ(texenv_entry->n_repeats,
              0u);
}

TEST(StateCensus, ACallThatAnotherInterceptorLeftOutIsCountedAndNotAsAKeyedCall)
{
    Stereo::Replay::ConsistencyChecker::StateCensus census({false});

    replay(&census, make_call(APIInterceptor::APIFUNCTION_GL_GLTEXPARAMETERI, {0x0DE1, 0x2801, 0x2601}), 0, false /* in_was_executed */);
    census.finish();

    const auto  entry_vec = census.get_entries();
    const auto* const entry = find_entry(entry_vec, APIInterceptor::APIFUNCTION_GL_GLTEXPARAMETERI);

    ASSERT_NE(entry, nullptr);

    EXPECT_EQ(entry->n_calls,
              1u);
    EXPECT_EQ(entry->n_skipped_by_others,
              1u);
    EXPECT_EQ(entry->n_keyed,
              0u);
}

TEST(StateCensus, AFrameWhosePushesAreAllPoppedAndWhichChangesNothingOutsideThemIsNeutral)
{
    Stereo::Replay::ConsistencyChecker::StateCensus census({false, false, false});

    /* Frame 0: all pushed, all popped. */
    replay(&census, make_call(APIInterceptor::APIFUNCTION_GL_GLPUSHATTRIB, {0xFFFFF}), 0);
    replay(&census, make_call(APIInterceptor::APIFUNCTION_GL_GLPUSHMATRIX),            0);
    replay(&census, make_call(APIInterceptor::APIFUNCTION_GL_GLENABLE,     {CAPABILITY_A}), 0);
    replay(&census, make_call(APIInterceptor::APIFUNCTION_GL_GLTRANSLATEF, {0, 0, 0}), 0);
    replay(&census, make_call(APIInterceptor::APIFUNCTION_GL_GLPOPMATRIX),             0);
    replay(&census, make_call(APIInterceptor::APIFUNCTION_GL_GLPOPATTRIB),             0);

    /* Frame 1: a capability is set outside the pushes. */
    replay(&census, make_call(APIInterceptor::APIFUNCTION_GL_GLENABLE,     {CAPABILITY_A}), 1);

    /* Frame 2: a push of attributes that is not popped. */
    replay(&census, make_call(APIInterceptor::APIFUNCTION_GL_GLPUSHATTRIB, {0xFFFFF}), 2);
    census.finish();

    EXPECT_EQ(census.get_n_dead_frames_with_calls(),
              3u);
    EXPECT_EQ(census.get_n_balanced_frames(),
              2u);
    EXPECT_EQ(census.get_n_neutral_frames(),
              1u);
    EXPECT_EQ(census.get_n_state_calls_outside_pushes(),
              1u);
    EXPECT_EQ(census.get_n_matrix_calls_outside_pushes(),
              0u);
    EXPECT_EQ(census.get_push_mask_count_map().at(0xFFFFF),
              2u);
}
