/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Checks of StateCoalescer and StateCoalescerVerifier on effects that the test makes up: which calls are made, in what order, with which selector
 * calls, and that a wrong choice or a call that is not understood ends the process.
 */
#include <gtest/gtest.h>
#include "analysis/api_dump_index/state_coalescer.h"
#include "analysis/api_dump_index/state_coalescer_verifier.h"
#include "common/macros.h"
#include <random>
#include <stdlib.h>

namespace
{
    using Capture::APIDumpLoader::DumpedAPICall;
    using Analysis::APIDumpIndex::StateCoalescer;
    using Analysis::APIDumpIndex::StateEffect;
    using Analysis::APIDumpIndex::StateEffectKind;

    constexpr uint32_t DOMAIN_A = 1u << 0;
    constexpr uint32_t DOMAIN_B = 1u << 1;
    constexpr uint32_t SELECTOR = 0;
    constexpr uint32_t OTHER    = 1;

    StateEffect assign(const uint32_t& in_domain,
                       const uint32_t& in_key,
                       const uint32_t& in_selector_mask = 0)
    {
        StateEffect result;

        result.domain_mask   = in_domain;
        result.key           = {{1, in_key, 0, 0}};
        result.kind          = StateEffectKind::ASSIGN;
        result.selector_mask = in_selector_mask;

        return result;
    }

    StateEffect barrier()
    {
        StateEffect result;

        result.kind = StateEffectKind::BARRIER;

        return result;
    }

    StateEffect ordered(const uint32_t& in_domain_mask,
                        const uint32_t& in_selector_mask = 0)
    {
        StateEffect result;

        result.domain_mask   = in_domain_mask;
        result.kind          = StateEffectKind::ORDERED;
        result.selector_mask = in_selector_mask;

        return result;
    }

    StateEffect choose_selector(const uint32_t& in_selector,
                       const uint32_t& in_value)
    {
        StateEffect result;

        result.kind           = StateEffectKind::SELECT;
        result.selector       = in_selector;
        result.selector_value = in_value;

        return result;
    }

    /* Calls that carry their number, and the effects that the test gives them. */
    std::vector<size_t> coalesce(const std::vector<StateEffect>& in_effect_vec,
                                 const std::vector<bool>&        in_is_kept_vec = std::vector<bool>())
    {
        std::vector<DumpedAPICall> call_vec(in_effect_vec.size() );
        std::vector<bool>          is_kept_vec = (in_is_kept_vec.empty() ) ? std::vector<bool>(in_effect_vec.size(),
                                                                                              true)
                                                                           : in_is_kept_vec;

        for (size_t n_call = 0;
                    n_call < call_vec.size();
                  ++n_call)
        {
            call_vec.at(n_call).func          = APIInterceptor::APIFUNCTION_GL_GLACCUM;
            call_vec.at(n_call).timestamp_qpc = n_call;
        }

        const auto describe_func = [&in_effect_vec](const DumpedAPICall& in_call)
        {
            return in_effect_vec.at(static_cast<size_t>(in_call.timestamp_qpc) );
        };

        StateCoalescer coalescer(describe_func);

        return coalescer.coalesce(call_vec,
                                  is_kept_vec);
    }

    void set_no_error_dialog()
    {
        ::_putenv_s("APIINTERCEPTOR_NO_ERROR_DIALOG",
                    "1");
    }
}

TEST(StateCoalescer, OfCallsThatSetOnePieceOfStateOnlyTheLastIsMade)
{
    const auto made_vec = coalesce({assign(DOMAIN_A, 1),
                                    assign(DOMAIN_A, 1),
                                    assign(DOMAIN_A, 2),
                                    barrier()});

    EXPECT_EQ(made_vec,
              std::vector<size_t>({1, 2, 3}) );
}

TEST(StateCoalescer, ACallThatIsKeptNotIsNotMade)
{
    const auto made_vec = coalesce({assign(DOMAIN_A, 1),
                                    assign(DOMAIN_A, 2)},
                                   {true, false});

    EXPECT_EQ(made_vec,
              std::vector<size_t>({0}) );
}

TEST(StateCoalescer, ABarrierMakesTheCallsBeforeItAndEndsWhatCanBeLeftOut)
{
    const auto made_vec = coalesce({assign(DOMAIN_A, 1),
                                    barrier(),
                                    assign(DOMAIN_A, 1)});

    EXPECT_EQ(made_vec,
              std::vector<size_t>({0, 1, 2}) );
}

TEST(StateCoalescer, ACallThatReadsADomainMakesTheCallsOfThatDomainBeforeItAndNoOther)
{
    const auto made_vec = coalesce({assign(DOMAIN_A, 1),
                                    assign(DOMAIN_B, 2),
                                    ordered(DOMAIN_A),
                                    assign(DOMAIN_A, 1)});

    /* The call of the domain it reads, then the call itself; the call of the other domain waits for the end. */
    EXPECT_EQ(made_vec,
              std::vector<size_t>({0, 2, 1, 3}) );
}

TEST(StateCoalescer, AChoiceIsMadeWhereACallNeedsItAndTheLastOneIsMadeAtTheEnd)
{
    const auto made_vec = coalesce({choose_selector(SELECTOR, 5),
                                    assign(DOMAIN_A, 1, 1u << SELECTOR),
                                    choose_selector(SELECTOR, 7),
                                    assign(DOMAIN_A, 2, 1u << SELECTOR),
                                    choose_selector(SELECTOR, 9)});

    /* Choices 5 and 7 where the calls need them, and 9 so that the selector is as it was after the segment. */
    EXPECT_EQ(made_vec,
              std::vector<size_t>({0, 1, 2, 3, 4}) );
}

TEST(StateCoalescer, TwoChoicesOfTheSameValueAreOneChoiceAndTheCallsAfterThemShareAPieceOfState)
{
    const auto made_vec = coalesce({choose_selector(SELECTOR, 5),
                                    assign(DOMAIN_A, 1, 1u << SELECTOR),
                                    choose_selector(SELECTOR, 5),
                                    assign(DOMAIN_A, 1, 1u << SELECTOR)});

    EXPECT_EQ(made_vec,
              std::vector<size_t>({2, 3}) );
}

TEST(StateCoalescer, TheSameKeyUnderAnotherChoiceIsAnotherPieceOfState)
{
    const auto made_vec = coalesce({choose_selector(SELECTOR, 5),
                                    assign(DOMAIN_A, 1, 1u << SELECTOR),
                                    choose_selector(SELECTOR, 6),
                                    assign(DOMAIN_A, 1, 1u << SELECTOR)});

    EXPECT_EQ(made_vec,
              std::vector<size_t>({0, 1, 2, 3}) );
}

TEST(StateCoalescer, ABarrierMakesTheSelectorsAsTheyWereAndLeavesNothingKnownAfterIt)
{
    const auto made_vec = coalesce({choose_selector(SELECTOR, 5),
                                    assign(DOMAIN_A, 1, 1u << SELECTOR),
                                    choose_selector(SELECTOR, 7),
                                    barrier(),
                                    assign(DOMAIN_A, 2, 1u << SELECTOR)});

    /* 5 for the call, then 7 so that the barrier sees what the workload had; after it nothing is known, so the last call needs no choice. */
    EXPECT_EQ(made_vec,
              std::vector<size_t>({0, 1, 2, 3, 4}) );
}

TEST(StateCoalescer, ACallBeforeAnyChoiceNeedsNoChoiceToBeMade)
{
    const auto made_vec = coalesce({assign(DOMAIN_A, 1, 1u << SELECTOR),
                                    choose_selector(SELECTOR, 5),
                                    assign(DOMAIN_A, 2, 1u << SELECTOR)});

    EXPECT_EQ(made_vec,
              std::vector<size_t>({0, 1, 2}) );
}

TEST(StateCoalescer, ACallThatMustBeMadeInPlaceHasTheChoiceItUsesMadeBeforeIt)
{
    const auto made_vec = coalesce({choose_selector(SELECTOR, 5),
                                    choose_selector(OTHER, 3),
                                    ordered(0, 1u << SELECTOR),
                                    choose_selector(SELECTOR, 6)});

    /* Only the selector that the call uses is made before it; the other one is made at the end, with the last choice of the first. */
    EXPECT_EQ(made_vec,
              std::vector<size_t>({0, 2, 3, 1}) );
}

TEST(StateCoalescer, CallsOfRandomWorkloadsAreCheckedByTheVerifierAndNeverMoreThanThreeTimesTheKeptOnes)
{
    constexpr uint32_t DOMAIN_C = 1u << 2;

    std::mt19937 random(12345);

    for (uint32_t n_run = 0;
                  n_run < 40000;
                ++n_run)
    {
        const uint32_t           n_calls = 4 + random() % 37;
        std::vector<StateEffect> effect_vec;

        for (uint32_t n_call = 0;
                      n_call < n_calls;
                    ++n_call)
        {
            const uint32_t domain_array[] = {DOMAIN_A, DOMAIN_B, DOMAIN_C};
            const uint32_t selector_mask  = random() % 4;

            switch (random() % 9)
            {
                case 0:
                case 1:
                case 2:
                {
                    effect_vec.push_back(assign(domain_array[random() % 3],
                                                random() % 3,
                                                selector_mask) );

                    break;
                }

                case 3:
                case 4:
                {
                    effect_vec.push_back(choose_selector(random() % 2,
                                                         random() % 3) );

                    break;
                }

                case 5:
                case 6:
                {
                    effect_vec.push_back(ordered(random() % 8,
                                                 selector_mask) );

                    break;
                }

                default:
                {
                    effect_vec.push_back(barrier() );

                    break;
                }
            }
        }

        /* The verifier is run inside: it ends the process if the choice is wrong. */
        const auto made_vec = coalesce(effect_vec);

        EXPECT_LE(made_vec.size(),
                  effect_vec.size() * 3);
    }
}

TEST(StateCoalescerVerifier, ALeftOutCallThatIsNeededEndsTheProcess)
{
    set_no_error_dialog();

    const std::vector<StateEffect> effect_vec  = {assign(DOMAIN_A, 1), barrier()};
    const std::vector<bool>        is_kept_vec = {true, true};

    EXPECT_EXIT(Analysis::APIDumpIndex::StateCoalescerVerifier::verify(effect_vec,
                                                                       is_kept_vec,
                                                                       std::vector<size_t>({1}) ),
                ::testing::ExitedWithCode(APIInterceptor::FatalError::get_exit_code() ),
                "would find it set by other calls");
}

TEST(StateCoalescerVerifier, ACallThatIsMadeOutOfOrderEndsTheProcess)
{
    set_no_error_dialog();

    const std::vector<StateEffect> effect_vec  = {ordered(0), barrier()};
    const std::vector<bool>        is_kept_vec = {true, true};

    EXPECT_EXIT(Analysis::APIDumpIndex::StateCoalescerVerifier::verify(effect_vec,
                                                                       is_kept_vec,
                                                                       std::vector<size_t>({1, 0}) ),
                ::testing::ExitedWithCode(APIInterceptor::FatalError::get_exit_code() ),
                "left out, repeated or reordered");
}

TEST(StateCoalescerVerifier, ASelectorThatAcallFindsWithAnotherChoiceEndsTheProcess)
{
    set_no_error_dialog();

    const std::vector<StateEffect> effect_vec  = {choose_selector(SELECTOR, 5), ordered(0, 1u << SELECTOR)};
    const std::vector<bool>        is_kept_vec = {true, true};

    /* The call that chooses is not made before the call that uses the choice. */
    EXPECT_EXIT(Analysis::APIDumpIndex::StateCoalescerVerifier::verify(effect_vec,
                                                                       is_kept_vec,
                                                                       std::vector<size_t>({1}) ),
                ::testing::ExitedWithCode(APIInterceptor::FatalError::get_exit_code() ),
                "uses a selector");
}

TEST(StateCoalescer, AnEffectThatIsNotWhatACallCanBeEndsTheProcess)
{
    set_no_error_dialog();

    StateEffect two_domains = assign(DOMAIN_A | DOMAIN_B,
                                     1);
    StateEffect no_family   = assign(DOMAIN_A,
                                     1);
    StateEffect big_value   = choose_selector(SELECTOR,
                                     0);

    no_family.key.at(0)    = 0;
    big_value.selector_value = 1ull << 62;

    EXPECT_EXIT(coalesce({two_domains}),
                ::testing::ExitedWithCode(APIInterceptor::FatalError::get_exit_code() ),
                "names no domain, or more than one");
    EXPECT_EXIT(coalesce({no_family}),
                ::testing::ExitedWithCode(APIInterceptor::FatalError::get_exit_code() ),
                "no kind of state in its key");
    EXPECT_EXIT(coalesce({big_value}),
                ::testing::ExitedWithCode(APIInterceptor::FatalError::get_exit_code() ),
                "out of range");
}

TEST(StateCoalescer, ACallThatEndsAFrameWhichIsNotABarrierEndsTheProcess)
{
    set_no_error_dialog();

    EXPECT_EXIT({
                    std::vector<DumpedAPICall> call_vec(1);

                    call_vec.at(0).func = APIInterceptor::APIFUNCTION_GDI32_SWAPBUFFERS;

                    const auto describe_func = [](const DumpedAPICall&)
                    {
                        return ordered(0);
                    };

                    StateCoalescer coalescer(describe_func);

                    coalescer.coalesce(call_vec,
                                       std::vector<bool>({true}) );
                },
                ::testing::ExitedWithCode(APIInterceptor::FatalError::get_exit_code() ),
                "ends a frame is not a barrier");
}

TEST(StateCoalescer, ACallOfADisplayListWhileCallsAreNotMadeYetEndsTheProcess)
{
    set_no_error_dialog();

    StateEffect recorded;

    recorded.kind = StateEffectKind::RECORDED;

    EXPECT_EXIT(coalesce({assign(DOMAIN_A, 1),
                          recorded}),
                ::testing::ExitedWithCode(APIInterceptor::FatalError::get_exit_code() ),
                "not made yet");
}
