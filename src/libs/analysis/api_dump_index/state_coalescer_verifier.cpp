/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_index/state_coalescer_verifier.h"
#include "common/macros.h"
#include <string>

void Analysis::APIDumpIndex::StateCoalescerVerifier::apply(const StateEffect& in_effect,
                                                           const size_t&      in_call_index,
                                                           Machine*           inout_machine_ptr)
{
    switch (in_effect.kind)
    {
        case StateEffectKind::ASSIGN:
        {
            const uint32_t domain_bit = get_domain_bit(in_effect.domain_mask);
            const auto     key        = resolve(in_effect,
                                                *inout_machine_ptr);

            inout_machine_ptr->writer_map_array.at(domain_bit)[key] = in_call_index;

            break;
        }

        case StateEffectKind::BARRIER:
        {
            for (auto& selector : inout_machine_ptr->selector_array)
            {
                selector.is_known = false;
            }

            inout_machine_ptr->n_barriers++;

            break;
        }

        case StateEffectKind::ORDERED:
        case StateEffectKind::RECORDED:
        {
            break;
        }

        case StateEffectKind::SELECT:
        {
            auto& selector = inout_machine_ptr->selector_array.at(in_effect.selector);

            selector.is_known = true;
            selector.value    = in_effect.selector_value;

            break;
        }

        default:
        {
            AI_FAIL_UNSUPPORTED(in_effect.kind);
        }
    }
}

void Analysis::APIDumpIndex::StateCoalescerVerifier::compare(const StateEffect& in_effect,
                                                             const size_t&      in_call_index,
                                                             const Machine&     in_original,
                                                             const Machine&     in_chosen)
{
    const bool is_barrier = (in_effect.kind == StateEffectKind::BARRIER);

    for (uint32_t n_selector = 0;
                  n_selector < N_SELECTORS;
                ++n_selector)
    {
        const bool is_read = (is_barrier                                       ||
                              (in_effect.selector_mask & (1u << n_selector) ) != 0);

        if (is_read)
        {
            const auto& original = in_original.selector_array.at(n_selector);
            const auto& chosen   = in_chosen.selector_array.at  (n_selector);

            const bool is_same = (original.is_known == chosen.is_known &&
                                  (original.is_known == false || original.value == chosen.value) );

            if (is_same == false)
            {
                const std::string message = "State coalescing: a call that uses a selector would find another choice than in the workload (call " +
                                            std::to_string(in_call_index)                                                                         +
                                            ").";

                AI_ASSERT_ALWAYS(false,
                                 message.c_str() );
            }
        }
    }

    for (uint32_t n_domain = 0;
                  n_domain < N_SELECTORS;
                ++n_domain)
    {
        const bool is_read = (is_barrier                                      ||
                              (in_effect.domain_mask & (1u << n_domain) ) != 0);

        if (is_read)
        {
            const bool is_same = (in_original.writer_map_array.at(n_domain) == in_chosen.writer_map_array.at(n_domain) );

            if (is_same == false)
            {
                const std::string message = "State coalescing: a call that reads state would find it set by other calls than in the workload (call " +
                                            std::to_string(in_call_index)                                                                          +
                                            ", domain "                                                                                            +
                                            std::to_string(n_domain)                                                                               +
                                            ").";

                AI_ASSERT_ALWAYS(false,
                                 message.c_str() );
            }
        }
    }

}

uint32_t Analysis::APIDumpIndex::StateCoalescerVerifier::get_domain_bit(const uint32_t& in_domain_mask)
{
    uint32_t n_bit = 0;

    AI_ASSERT_ALWAYS(in_domain_mask != 0 &&
                     (in_domain_mask & (in_domain_mask - 1) ) == 0,
                     "State coalescing: a call that sets state names no domain, or more than one.");

    while ( (in_domain_mask & (1u << n_bit) ) == 0)
    {
        n_bit++;
    }

    return n_bit;
}

Analysis::APIDumpIndex::StateCoalescerVerifier::ResolvedKey Analysis::APIDumpIndex::StateCoalescerVerifier::resolve(const StateEffect& in_effect,
                                                                                                                    const Machine&     in_machine)
{
    constexpr uint64_t UNKNOWN_CHOICE_BASE = 1ull << 62;

    ResolvedKey result;

    result.first = in_effect.key;

    for (uint32_t n_selector = 0;
                  n_selector < N_SELECTORS;
                ++n_selector)
    {
        if ( (in_effect.selector_mask & (1u << n_selector) ) != 0)
        {
            const auto& selector = in_machine.selector_array.at(n_selector);

            result.second.push_back( (selector.is_known) ? selector.value
                                                         : UNKNOWN_CHOICE_BASE + in_machine.n_barriers);
        }
    }

    return result;
}

void Analysis::APIDumpIndex::StateCoalescerVerifier::verify(const std::vector<StateEffect>& in_effect_vec,
                                                            const std::vector<bool>&        in_is_kept_vec,
                                                            const std::vector<size_t>&      in_chosen_vec)
{
    Machine             chosen;
    std::vector<size_t> chosen_observer_vec;
    std::vector<size_t> kept_vec;
    Machine             original;
    std::vector<size_t> original_observer_vec;

    AI_ASSERT_ALWAYS(in_effect_vec.size() == in_is_kept_vec.size(),
                     "State coalescing: the effects and the kept calls differ in number.");

    for (size_t n_call = 0;
                n_call < in_is_kept_vec.size();
              ++n_call)
    {
        if (in_is_kept_vec.at(n_call) )
        {
            kept_vec.push_back(n_call);

            const auto kind = in_effect_vec.at(n_call).kind;

            if (kind != StateEffectKind::ASSIGN &&
                kind != StateEffectKind::SELECT)
            {
                original_observer_vec.push_back(n_call);
            }
        }
    }

    for (const auto& call_index : in_chosen_vec)
    {
        AI_ASSERT_ALWAYS(call_index < in_is_kept_vec.size() &&
                         in_is_kept_vec.at(call_index),
                         "State coalescing chose a call that is not one of the kept calls.");

        const auto kind = in_effect_vec.at(call_index).kind;

        if (kind != StateEffectKind::ASSIGN &&
            kind != StateEffectKind::SELECT)
        {
            chosen_observer_vec.push_back(call_index);
        }
    }

    /* Calls that are not coalesced are made once each, in the order they were. */
    AI_ASSERT_ALWAYS(chosen_observer_vec == original_observer_vec,
                     "State coalescing left out, repeated or reordered a call that it must make as it was.");

    size_t n_chosen = 0;
    size_t n_kept   = 0;

    for (const auto& observer_index : original_observer_vec)
    {
        while (kept_vec.at(n_kept) != observer_index)
        {
            apply(in_effect_vec.at(kept_vec.at(n_kept) ),
                  kept_vec.at(n_kept),
                 &original);

            n_kept++;
        }

        while (in_chosen_vec.at(n_chosen) != observer_index)
        {
            apply(in_effect_vec.at(in_chosen_vec.at(n_chosen) ),
                  in_chosen_vec.at(n_chosen),
                 &chosen);

            n_chosen++;
        }

        compare(in_effect_vec.at(observer_index),
                observer_index,
                original,
                chosen);

        apply(in_effect_vec.at(observer_index),
              observer_index,
             &original);
        apply(in_effect_vec.at(observer_index),
              observer_index,
             &chosen);

        n_chosen++;
        n_kept++;
    }

    /* What is left after the last call that reads state is state that the calls after the segment read. */
    for (; n_kept < kept_vec.size(); ++n_kept)
    {
        apply(in_effect_vec.at(kept_vec.at(n_kept) ),
              kept_vec.at(n_kept),
             &original);
    }

    for (; n_chosen < in_chosen_vec.size(); ++n_chosen)
    {
        apply(in_effect_vec.at(in_chosen_vec.at(n_chosen) ),
              in_chosen_vec.at(n_chosen),
             &chosen);
    }

    StateEffect everything;

    everything.kind = StateEffectKind::BARRIER;

    compare(everything,
            kept_vec.empty() ? 0 : kept_vec.back(),
            original,
            chosen);
}
