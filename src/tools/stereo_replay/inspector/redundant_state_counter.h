/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Observer that counts, per OpenGL function, calls in dead frames that set state to its current value or are overwritten before being read.
 * A piece of state is a function plus its selecting arguments. Barriers (list calls, attribute push/pop, queries, deletes, context
 * changes) end a run of calls that could be merged; matrix, bind and make-object calls are not counted.
 */
#ifndef STEREO_REPLAY_INSPECTOR_REDUNDANT_STATE_COUNTER_H
#define STEREO_REPLAY_INSPECTOR_REDUNDANT_STATE_COUNTER_H

#include "replay/replay_engine_dll/i_call_interceptor.h"
#include "types.h"
#include <map>
#include <stdint.h>
#include <string>
#include <vector>

namespace Stereo
{
    namespace Replay
    {
        namespace Inspector
        {
            class RedundantStateCounter : public ::Replay::ReplayEngineDLL::ICallInterceptor
            {
            public:
                /* Counts of the calls of one function in the dead frames. */
                struct FunctionEntry
                {
                    StateCallClass              call_class          = StateCallClass::OTHER;
                    APIInterceptor::APIFunction function            = APIInterceptor::APIFUNCTION_UNKNOWN;
                    uint64_t                    n_calls             = 0; /* all seen                                              */
                    uint64_t                    n_in_lists          = 0; /* recorded into a list being made                       */
                    uint64_t                    n_keyed             = 0; /* set a piece of state                                  */
                    uint64_t                    n_overwritten_later = 0; /* keyed, overwritten before a made frame or a barrier   */
                    uint64_t                    n_repeats           = 0; /* keyed, set the value the state already has            */
                    uint64_t                    n_skipped_by_others = 0; /* left out before this saw them (texture parameter cache) */
                };

                /* Indexed by frame number: false is dead; frames past the end are live. */
                explicit RedundantStateCounter(const std::vector<bool>& in_is_live_vec);

                /* Closes the last frame seen; call when the replay is over. */
                void finish();

                /* Frames that leave the state as before: balanced pushes/pops and no state or matrix calls outside them (bound and made objects are not tracked). */
                uint64_t get_n_neutral_frames() const;

                /* Dead frames with calls; those with balanced attribute/matrix pushes; calls outside pushes. */
                uint64_t get_n_balanced_frames            () const;
                uint64_t get_n_dead_frames_with_calls     () const;
                uint64_t get_n_matrix_calls_outside_pushes() const;
                uint64_t get_n_state_calls_outside_pushes () const;

                /* Attribute push mask to push count, dead frames. */
                const std::map<uint64_t, uint64_t>& get_push_mask_count_map() const;

                /* Most calls first. */
                std::vector<FunctionEntry> get_entries() const;

                /* Barrier function name to count. */
                const std::map<std::string, uint64_t>& get_barrier_count_map() const;

                /* Pieces of state a run ends with a different value than it began with, over all runs (an unknown value counts as different). */
                uint64_t get_n_changed_keys() const;

                /* Runs with at least one keyed call; a barrier or a made frame ends a run. */
                uint64_t get_n_runs() const;

                /* Pieces of state a run ends with the value it began with, over all runs. */
                uint64_t get_n_unchanged_keys() const;

                void on_call_replayed(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                      const uint32_t&                              in_n_frame,
                                      const bool&                                  in_was_executed) override;

                bool should_execute_call(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                         const uint32_t&                              in_n_frame) override;

            private:
                /* Numbers a piece of state was set to, if all are known. */
                struct StateValue
                {
                    bool                has_value = false;
                    std::vector<double> value_vec;
                };

                /* State set in the run: last setting function and the value before the run. */
                struct PendingEntry
                {
                    APIInterceptor::APIFunction function = APIInterceptor::APIFUNCTION_UNKNOWN;
                    StateValue                  value_before_run;
                };

                /* A call as classified for the census. */
                struct ClassifiedCall
                {
                    StateCallClass call_class = StateCallClass::OTHER;
                    std::string    key;
                    StateValue     value;
                };

                /* Returns the call's effect on state, the piece of state it sets and the value. */
                ClassifiedCall classify(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                        const char*                                  in_name_ptr) const;

                /* Counts the frame as balanced, neutral or neither. */
                void end_frame();

                /* Ends the current run of mergeable calls. */
                void end_run();

                /* Tracks attribute and matrix pushes and state changes outside them. */
                void follow_frame(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                  const char*                                  in_name_ptr,
                                  const ClassifiedCall&                        in_classified);

                static bool has_prefix (const char*        in_name_ptr,
                                        const char*        in_prefix_ptr);
                static bool is_one_of  (const char* const* in_name_ptr_array,
                                        const size_t&      in_n_names,
                                        const char*        in_name_ptr);
                static bool takes_array(const char*        in_name_ptr);

                uint32_t                                             m_active_unit;
                int64_t                                              m_attribute_depth;            /* last frame seen: unpopped attribute pushes */
                std::map<std::string, uint64_t>                      m_barrier_count_map;
                uint32_t                                             m_client_unit;
                std::map<APIInterceptor::APIFunction, FunctionEntry> m_entry_map;
                int64_t                                              m_frame_matrix_calls_outside; /* last frame seen */
                int64_t                                              m_frame_state_calls_outside;  /* last frame seen */
                bool                                                 m_has_frame;
                std::vector<bool>                                    m_is_live_vec;
                bool                                                 m_is_recording;
                uint64_t                                             m_last_context;
                int64_t                                              m_matrix_depth;
                uint64_t                                             m_n_balanced_frames;
                uint64_t                                             m_n_dead_frames_with_calls;
                uint64_t                                             m_n_matrix_calls_outside;
                uint64_t                                             m_n_neutral_frames;
                uint64_t                                             m_n_runs;
                uint64_t                                             m_n_runs_changed_keys;        /* keys that ended a run changed   */
                uint64_t                                             m_n_runs_unchanged_keys;      /* keys that ended a run unchanged */
                uint64_t                                             m_n_state_calls_outside;
                std::map<std::string, PendingEntry>                  m_pending_map;                /* by piece of state, in the run      */
                std::map<uint64_t, uint64_t>                         m_push_mask_count_map;
                uint32_t                                             m_seen_frame;
                std::map<std::string, StateValue>                    m_state_map;                  /* state so far, by piece of state    */
                std::map<std::pair<uint32_t, uint32_t>, uint32_t>    m_texture_binding_map;        /* bound texture by unit and target  */
            };
        }
    }
}

#endif /* STEREO_REPLAY_INSPECTOR_REDUNDANT_STATE_COUNTER_H */
