/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Chooses which of the calls that a replay of dead frames makes are needed. A dead frame makes no content, so nothing reads the state that its
 * calls set until a call that reads state: of the calls that set the same piece of state, only the last before such a call is made. What it
 * chooses is checked by StateCoalescerVerifier, and a call that the describer does not know ends the process (see IStateEffectDescriber).
 *
 * What a call sets and reads is told by a function (see StateEffect). A call that is not coalesced is made once, in its place; one that is
 * chosen after the calls that came before it is made with the selector calls it needs (a copy of a call of the workload, made again), and the
 * selectors are as they were when the calls before a barrier, a frame or the segment are done.
 */
#ifndef APIDUMPINDEX_STATE_COALESCER_H
#define APIDUMPINDEX_STATE_COALESCER_H

#include "analysis/api_dump_index/types.h"
#include "capture/api_dump_loader/api_dump_loader.h"
#include <array>
#include <functional>
#include <map>
#include <tuple>
#include <vector>

namespace Analysis
{
    namespace APIDumpIndex
    {
        class StateCoalescer
        {
        public:
            /* Says what a call does to state; given the kept calls in order, once each. */
            typedef std::function<StateEffect(const Capture::APIDumpLoader::DumpedAPICall&)> DescribeFunc;

            explicit StateCoalescer(const DescribeFunc& in_describe_func);

            /* The calls to make, as indices into in_call_vec in the order to make them, of the kept calls (those of in_is_kept_vec): a call may be made
             * again, and one may be left out. The state after them is the state after all the kept calls.
             */
            std::vector<size_t> coalesce(const std::vector<Capture::APIDumpLoader::DumpedAPICall>& in_call_vec,
                                         const std::vector<bool>&          in_is_kept_vec);

        private:
            static const uint32_t N_SELECTORS = 32;

            /* What a selector holds: nothing known (as the start, or a barrier left it), or a value, and the call that chose it. */
            struct Choice
            {
                size_t   call_index = 0;
                bool     is_known   = false;
                uint64_t value      = 0;
            };

            /* A call that sets a piece of state and is not made yet, with the choices of the selectors it uses. */
            struct Pending
            {
                size_t                                     call_index = 0;
                std::vector<std::pair<uint32_t, Choice> >  choice_vec;
                uint32_t                                   domain_mask = 0;
            };

            /* A piece of state: its domain, its key, and the value that each selector it uses holds (UINT64_MAX when unknown). */
            typedef std::tuple<uint32_t, StateKey, std::vector<uint64_t> > PendingKey;

            /* What is going on in one run over the calls. */
            struct Run
            {
                std::array<Choice, N_SELECTORS> made_array;     /* what the calls made so far hold: where the replay is */
                std::vector<size_t>             made_vec;
                std::map<PendingKey, Pending>   pending_map;
                std::array<Choice, N_SELECTORS> workload_array; /* what the calls of the workload so far hold               */
            };

            /* Makes a call that sets the choice that the selector must hold, if it holds another one. */
            static void choose(const uint32_t& in_selector,
                               const Choice&   in_required,
                               Run*            inout_run_ptr);

            /* Makes all the selectors hold the choices, though making one can change another (the calls that come before it are made first). */
            static void choose_all(const std::vector<std::pair<uint32_t, Choice> >& in_choice_vec,
                                   Run*                                             inout_run_ptr);

            /* Makes the pending calls of the domains that were made in the workload up to the call, in the order they were made there. */
            static void flush(const uint32_t& in_domain_mask,
                              const size_t&   in_last_call_index,
                              Run*            inout_run_ptr);

            /* Makes all the pending calls, and the selectors hold what they held in the workload. */
            static void flush_all(Run* inout_run_ptr);

            /* Before a selector is first chosen, makes the pending calls that need what it held before: they and the calls of their domains that came before them. */
            static void flush_unknown(const uint32_t& in_selector,
                                      Run*            inout_run_ptr);

            static void make(const size_t& in_call_index,
                             Run*          inout_run_ptr);

            /* Makes the selectors hold what the calls of the workload hold. */
            static void restore(const uint32_t& in_selector_mask,
                                Run*            inout_run_ptr);

            /* Ends the process if the effect cannot be what a describer says of the call. */
            static void validate(const StateEffect&    in_effect,
                                 const Capture::APIDumpLoader::DumpedAPICall&  in_call);

            DescribeFunc m_describe_func;
        };
    }
}

#endif /* APIDUMPINDEX_STATE_COALESCER_H */
