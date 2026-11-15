/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Counts what the OpenGL calls in the dead frames of a seek do to the state of the context, and how many of them need not be made. For
 * each function it reports how many of its calls set a piece of state (a capability, a material, a texture parameter, ...) to the value
 * that it already has (a state cache, such as the cache of texture parameters, leaves those out), and how many are overwritten by another
 * call of the same piece of state before anything that is made reads it (those need not be made at all if only the last call is made when
 * the next frame that is made begins). The census only observes: it changes nothing, and the frames that the slice says are dead tell it
 * what to count.
 *
 * The state is followed call by call, in replay order, by the name of each function and its arguments. A piece of state is a function (its
 * forms that take a number or an array count as one) plus the arguments that select it (the capability; the face and name of a material;
 * the texture unit and the bound texture for a texture parameter). Calls that set the matrices and their stack, bind a texture, or make an
 * object depend on earlier calls and are not counted as such. A call that reads the state, or may change it without being followed, ends a
 * run of calls that can be taken together (a barrier). Barriers are: a call of a list that is made, a push or pop of attributes, a query,
 * deleting a texture or a list, and making a context current.
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
                    uint64_t                    n_calls             = 0; /* all that were seen                                                                   */
                    uint64_t                    n_in_lists          = 0; /* of them, recorded into a list that is being made: they are kept in it                */
                    uint64_t                    n_keyed             = 0; /* of them, calls that set a piece of state                                             */
                    uint64_t                    n_overwritten_later = 0; /* of the keyed, overwritten by another call before a frame that is made or a barrier */
                    uint64_t                    n_repeats           = 0; /* of the keyed, calls that set the value the state already has                         */
                    uint64_t                    n_skipped_by_others = 0; /* of them, left out before this saw them made (the cache of texture parameters)        */
                };

                /* The frames are given by their numbers: those that are not live are dead; a frame past the end of the vector is live. */
                explicit RedundantStateCounter(const std::vector<bool>& in_is_live_vec);

                /* The frame that was last seen is done with. Called when the replay is over. */
                void finish();

                /* The number of frames whose calls change nothing that lasts. Every push of attributes and of a matrix has its pop, and no call that
                 * sets state or a matrix comes outside the pushes, so the state is what it was before the frame (except for what is bound and made, which
                 * is not tracked here).
                 */
                uint64_t get_n_neutral_frames() const;

                /* The number of dead frames that had calls; of them, the number whose pushes and pops of attributes and of matrices balance; and the
                 * number of calls outside the pushes. */
                uint64_t get_n_balanced_frames            () const;
                uint64_t get_n_dead_frames_with_calls     () const;
                uint64_t get_n_matrix_calls_outside_pushes() const;
                uint64_t get_n_state_calls_outside_pushes () const;

                /* The masks of the attribute pushes in the dead frames, and how many pushes each mask has. */
                const std::map<uint64_t, uint64_t>& get_push_mask_count_map() const;

                /* The functions seen in the dead frames, the function with the most calls first. */
                std::vector<FunctionEntry> get_entries() const;

                /* How often each kind of barrier occurred in the dead frames, by the name of the function. */
                const std::map<std::string, uint64_t>& get_barrier_count_map() const;

                /* The number of pieces of state that must be set when the runs are over, if only the last value of each piece of state in a run is made:
                 * the pieces that end a run with another value than they began it with, counted for all runs. A value that is not known to be the same
                 * counts as different.
                 */
                uint64_t get_n_changed_keys() const;

                /* The number of runs of calls that can be taken together. A barrier or a frame that is made ends a run; only runs that had a keyed call
                 * are counted. */
                uint64_t get_n_runs() const;

                /* The number of pieces of state that runs end with the value they began with, counted for all runs. The last call of each is not needed
                 * either. */
                uint64_t get_n_unchanged_keys() const;

                void on_call_replayed(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                      const uint32_t&                              in_n_frame,
                                      const bool&                                  in_was_executed) override;

                bool should_execute_call(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                         const uint32_t&                              in_n_frame) override;

            private:
                /* What a piece of state has been set to, as far as the calls say: its numbers, if they are all known. */
                struct StateValue
                {
                    bool                has_value = false;
                    std::vector<double> value_vec;
                };

                /* A piece of state that was set in the run: the function of the last call that set it, and its value before the run. */
                struct PendingEntry
                {
                    APIInterceptor::APIFunction function = APIInterceptor::APIFUNCTION_UNKNOWN;
                    StateValue                  value_before_run;
                };

                /* The census's view of a call. */
                struct ClassifiedCall
                {
                    StateCallClass call_class = StateCallClass::OTHER;
                    std::string    key;
                    StateValue     value;
                };

                /* Classifies a call: what it does to the state, the piece of state that it sets, and the value. */
                ClassifiedCall classify(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                        const char*                                  in_name_ptr) const;

                /* The frame is done with: it counts as balanced or as neutral, or as neither. */
                void end_frame();

                /* The current run of calls that may be taken together is over: the next call that is made sees the state as it is. */
                void end_run();

                /* Follows what a call made in a dead frame does to the pushes of attributes and of matrices, and to the state that is changed outside of
                 * them. */
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
                int64_t                                              m_attribute_depth;            /* of the frame that was last seen: the attribute pushes that are not yet popped */
                std::map<std::string, uint64_t>                      m_barrier_count_map;
                uint32_t                                             m_client_unit;
                std::map<APIInterceptor::APIFunction, FunctionEntry> m_entry_map;
                int64_t                                              m_frame_matrix_calls_outside; /* of the frame that was last seen                                          */
                int64_t                                              m_frame_state_calls_outside;  /* of the frame that was last seen                                          */
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
                uint64_t                                             m_n_runs_changed_keys;        /* pieces of state that the runs ended with a different value than they began with */
                uint64_t                                             m_n_runs_unchanged_keys;      /* pieces of state that the runs ended with the value they began with        */
                uint64_t                                             m_n_state_calls_outside;
                std::map<std::string, PendingEntry>                  m_pending_map;                /* by piece of state, in the run                                             */
                std::map<uint64_t, uint64_t>                         m_push_mask_count_map;
                uint32_t                                             m_seen_frame;
                std::map<std::string, StateValue>                    m_state_map;                  /* the state as the calls so far have made it, by piece of state             */
                std::map<std::pair<uint32_t, uint32_t>, uint32_t>    m_texture_binding_map;        /* the texture bound, by unit and target                                     */
            };
        }
    }
}

#endif /* STEREO_REPLAY_INSPECTOR_REDUNDANT_STATE_COUNTER_H */
