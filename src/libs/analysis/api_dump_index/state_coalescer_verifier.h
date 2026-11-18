/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Checks the work of StateCoalescer on its own: plays the calls as they were and the calls that were chosen on a model of state (what each piece
 * of state was last set by, what each selector holds), and ends the process if a call that reads state would see something else.
 */
#ifndef APIDUMPINDEX_STATE_COALESCER_VERIFIER_H
#define APIDUMPINDEX_STATE_COALESCER_VERIFIER_H

#include "analysis/api_dump_index/types.h"
#include <array>
#include <map>
#include <stddef.h>
#include <stdint.h>
#include <vector>

namespace Analysis
{
    namespace APIDumpIndex
    {
        class StateCoalescerVerifier
        {
        public:
            /* The effects are by call; the kept calls are those that the replay makes; the chosen calls are indices of kept calls, in the order they
             * are made (one may be made again). Ends the process if the two ways of making the calls differ for any call that reads state.
             */
            static void verify(const std::vector<StateEffect>& in_effect_vec,
                               const std::vector<bool>&        in_is_kept_vec,
                               const std::vector<size_t>&      in_chosen_vec);

        private:
            static const uint32_t N_SELECTORS = 32;

            /* What a selector holds: nothing known (as a barrier or the start left it), or a value. */
            struct SelectorValue
            {
                bool     is_known = false;
                uint64_t value    = 0;
            };

            /* What a piece of state was set by: the key, and the choice of each selector it depends on. */
            typedef std::pair<StateKey, std::vector<uint64_t> > ResolvedKey;

            /* The model of state that calls are played on. */
            struct Machine
            {
                uint64_t                                                     n_barriers = 0;   /* tells one unknown choice from another */
                std::array<SelectorValue, N_SELECTORS>                       selector_array;
                std::array<std::map<ResolvedKey, size_t>, N_SELECTORS>       writer_map_array; /* by domain: the call that set each piece */
            };

            static void apply(const StateEffect& in_effect,
                              const size_t&      in_call_index,
                              Machine*           inout_machine_ptr);

            /* Ends the process if the machines differ in what a call with this effect reads (all of it for a barrier). */
            static void compare(const StateEffect& in_effect,
                                const size_t&      in_call_index,
                                const Machine&     in_original,
                                const Machine&     in_chosen);

            static uint32_t get_domain_bit(const uint32_t& in_domain_mask);

            static ResolvedKey resolve(const StateEffect& in_effect,
                                       const Machine&     in_machine);
        };
    }
}

#endif /* APIDUMPINDEX_STATE_COALESCER_VERIFIER_H */
