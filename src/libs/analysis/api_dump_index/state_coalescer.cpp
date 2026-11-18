/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_index/state_coalescer.h"
#include "analysis/api_dump_index/state_coalescer_verifier.h"
#include "capture/api_dump_loader/api_info.h"
#include "common/macros.h"
#include <algorithm>

Analysis::APIDumpIndex::StateCoalescer::StateCoalescer(const DescribeFunc& in_describe_func)
    :m_describe_func(in_describe_func)
{
    /* Stub */
}

std::vector<size_t> Analysis::APIDumpIndex::StateCoalescer::coalesce(const std::vector<Capture::APIDumpLoader::DumpedAPICall>& in_call_vec,
                                                                     const std::vector<bool>&          in_is_kept_vec)
{
    std::vector<StateEffect> effect_vec(in_call_vec.size() );
    Run                      run;

    AI_ASSERT_ALWAYS(in_is_kept_vec.size() == in_call_vec.size(),
                     "State coalescing: the kept calls are not given one flag for each call.");

    for (size_t n_call = 0;
                n_call < in_call_vec.size();
              ++n_call)
    {
        if (in_is_kept_vec.at(n_call) == false)
        {
            continue;
        }

        const StateEffect effect = m_describe_func(in_call_vec.at(n_call) );

        validate(effect,
                 in_call_vec.at(n_call) );

        effect_vec.at(n_call) = effect;

        switch (effect.kind)
        {
            case StateEffectKind::ASSIGN:
            {
                Pending               pending;
                std::vector<uint64_t> value_vec;

                pending.call_index   = n_call;
                pending.domain_mask  = effect.domain_mask;

                for (uint32_t n_selector = 0;
                              n_selector < N_SELECTORS;
                            ++n_selector)
                {
                    if ( (effect.selector_mask & (1u << n_selector) ) != 0)
                    {
                        const Choice& workload = run.workload_array.at(n_selector);

                        value_vec.push_back( (workload.is_known) ? workload.value
                                                                 : UINT64_MAX);
                        pending.choice_vec.emplace_back(n_selector,
                                                        workload);
                    }
                }

                /* A call that sets the same piece of state the same way replaces the one before it. */
                run.pending_map[PendingKey(effect.domain_mask,
                                           effect.key,
                                           value_vec)] = pending;

                break;
            }

            case StateEffectKind::BARRIER:
            {
                flush_all(&run);
                make     (n_call,
                         &run);

                /* A barrier may change what the selectors hold (a context, a restored attribute): nothing is known after it. */
                run.made_array.fill    (Choice() );
                run.workload_array.fill(Choice() );

                break;
            }

            case StateEffectKind::ORDERED:
            {
                flush  (effect.domain_mask,
                        SIZE_MAX,
                       &run);
                restore(effect.selector_mask,
                       &run);
                make   (n_call,
                       &run);

                break;
            }

            case StateEffectKind::RECORDED:
            {
                AI_ASSERT_ALWAYS(run.pending_map.empty(),
                                 "State coalescing: a call of a display list is met while calls that set state are not made yet.");

                make(n_call,
                    &run);

                break;
            }

            case StateEffectKind::SELECT:
            {
                Choice& workload = run.workload_array.at(effect.selector);

                workload.call_index = n_call;
                workload.is_known   = true;
                workload.value      = effect.selector_value;

                break;
            }

            default:
            {
                AI_FAIL_UNSUPPORTED(effect.kind);
            }
        }
    }

    flush_all(&run);

    StateCoalescerVerifier::verify(effect_vec,
                                   in_is_kept_vec,
                                   run.made_vec);

    return run.made_vec;
}

void Analysis::APIDumpIndex::StateCoalescer::choose(const uint32_t& in_selector,
                                                    const Choice&   in_required,
                                                    Run*            inout_run_ptr)
{
    Choice& made = inout_run_ptr->made_array.at(in_selector);

    if (in_required.is_known)
    {
        /* The calls that need what the selector held before come first. */
        if (made.is_known == false)
        {
            flush_unknown(in_selector,
                          inout_run_ptr);
        }

        if (made.is_known == false ||
            made.value    != in_required.value)
        {
            make(in_required.call_index,
                 inout_run_ptr);

            made = in_required;
        }
    }
    else
    {
        /* Calls come in the order of the workload, so a call that needs what a barrier left is made before any call that chooses. */
        AI_ASSERT_ALWAYS(made.is_known == false,
                         "State coalescing: a call needs the choice that a barrier left, but another choice was made.");
    }
}

void Analysis::APIDumpIndex::StateCoalescer::choose_all(const std::vector<std::pair<uint32_t, Choice> >& in_choice_vec,
                                                        Run*                                             inout_run_ptr)
{
    size_t n_made_before = 0;

    /* Making one choice can make the calls that come before it, which can choose another selector: go on until a whole round makes nothing. */
    do
    {
        n_made_before = inout_run_ptr->made_vec.size();

        for (const auto& choice : in_choice_vec)
        {
            choose(choice.first,
                   choice.second,
                   inout_run_ptr);
        }
    }
    while (inout_run_ptr->made_vec.size() != n_made_before);
}

void Analysis::APIDumpIndex::StateCoalescer::flush(const uint32_t& in_domain_mask,
                                                   const size_t&   in_last_call_index,
                                                   Run*            inout_run_ptr)
{
    std::vector<std::pair<size_t, PendingKey> > due_vec;

    for (const auto& entry : inout_run_ptr->pending_map)
    {
        if ( (entry.second.domain_mask & in_domain_mask) != 0 &&
            entry.second.call_index <= in_last_call_index)
        {
            due_vec.emplace_back(entry.second.call_index,
                                 entry.first);
        }
    }

    const auto is_earlier_func = [](const std::pair<size_t, PendingKey>& in_left,
                                    const std::pair<size_t, PendingKey>& in_right)
    {
        return (in_left.first < in_right.first);
    };

    std::sort(due_vec.begin(),
              due_vec.end  (),
              is_earlier_func);

    for (const auto& due : due_vec)
    {
        const auto pending_iterator = inout_run_ptr->pending_map.find(due.second);

        /* Choosing may have made it already: it is one of the calls that were before another one. */
        if (pending_iterator == inout_run_ptr->pending_map.end() )
        {
            continue;
        }

        const Pending pending = pending_iterator->second;

        inout_run_ptr->pending_map.erase(pending_iterator);

        choose_all(pending.choice_vec,
                   inout_run_ptr);

        make(pending.call_index,
             inout_run_ptr);
    }
}

void Analysis::APIDumpIndex::StateCoalescer::flush_all(Run* inout_run_ptr)
{
    flush  (UINT32_MAX,
            SIZE_MAX,
            inout_run_ptr);
    restore(UINT32_MAX,
            inout_run_ptr);

    AI_ASSERT_ALWAYS(inout_run_ptr->pending_map.empty(),
                     "State coalescing: calls that set state are left after all were made.");
}

void Analysis::APIDumpIndex::StateCoalescer::flush_unknown(const uint32_t& in_selector,
                                                           Run*            inout_run_ptr)
{
    /* By domain, the last of the pending calls that need what the selector held before it was chosen. */
    std::map<uint32_t, size_t> last_call_index_map;

    for (const auto& entry : inout_run_ptr->pending_map)
    {
        for (const auto& choice : entry.second.choice_vec)
        {
            if (choice.first == in_selector &&
                choice.second.is_known == false)
            {
                size_t& last_call_index = last_call_index_map[entry.second.domain_mask];

                last_call_index = (std::max)(last_call_index,
                                             entry.second.call_index);
            }
        }
    }

    for (const auto& last : last_call_index_map)
    {
        flush(last.first,
              last.second,
              inout_run_ptr);
    }
}

void Analysis::APIDumpIndex::StateCoalescer::make(const size_t& in_call_index,
                                                  Run*          inout_run_ptr)
{
    inout_run_ptr->made_vec.push_back(in_call_index);
}

void Analysis::APIDumpIndex::StateCoalescer::restore(const uint32_t& in_selector_mask,
                                                     Run*            inout_run_ptr)
{
    std::vector<std::pair<uint32_t, Choice> > choice_vec;

    for (uint32_t n_selector = 0;
                  n_selector < N_SELECTORS;
                ++n_selector)
    {
        if ( (in_selector_mask & (1u << n_selector) ) != 0)
        {
            choice_vec.emplace_back(n_selector,
                                    inout_run_ptr->workload_array.at(n_selector) );
        }
    }

    choose_all(choice_vec,
               inout_run_ptr);
}

void Analysis::APIDumpIndex::StateCoalescer::validate(const StateEffect&   in_effect,
                                                      const Capture::APIDumpLoader::DumpedAPICall& in_call)
{
    constexpr uint64_t MAX_SELECTOR_VALUE = 1ull << 62; /* above it, values stand for choices that are not known (see StateCoalescerVerifier) */

    /* The end of a frame separates the frames of the replay, so everything is made before it. */
    AI_ASSERT_ALWAYS(Capture::APIDumpLoader::is_frame_boundary(in_call) == false ||
                     in_effect.kind == StateEffectKind::BARRIER,
                     "State coalescing: a call that ends a frame is not a barrier.");

    if (in_effect.kind == StateEffectKind::ASSIGN)
    {
        AI_ASSERT_ALWAYS(in_effect.domain_mask != 0 &&
                         (in_effect.domain_mask & (in_effect.domain_mask - 1) ) == 0,
                         "State coalescing: a call that sets state names no domain, or more than one.");
        AI_ASSERT_ALWAYS(in_effect.key.at(0) != 0,
                         "State coalescing: a call that sets state has no kind of state in its key.");
    }

    if (in_effect.kind == StateEffectKind::SELECT)
    {
        AI_ASSERT_ALWAYS(in_effect.selector < N_SELECTORS &&
                         in_effect.selector_value < MAX_SELECTOR_VALUE,
                         "State coalescing: a call that chooses names a selector or a value that is out of range.");
    }
}
