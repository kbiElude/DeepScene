/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * What the calls of OpenGL in the dead frames of a seek do to the state of the context, and how many of them need not be made: it says, for each function,
 * how many of its calls give a piece of state (a capability, a material, a texture parameter, ...) the value it already has (those are left out by a cache of
 * the state, as the cache of texture parameters does), and how many are overwritten by another call of the same piece of state before anything that is made
 * looks at it (those need not be made at all if only the last of them is made when the next frame that is made begins). It only looks: it makes nothing
 * different, and it is told what to count by the frames that the slice says are dead.
 *
 * The state is followed as the calls come, in the order they are replayed in, by the name of each function and its arguments: a piece of state is the
 * function (its forms that take a number or an array are one) and the arguments that say which (the capability, the face and the name of a material, the
 * texture unit and the texture that is bound for a texture parameter). Calls that set the matrices and the stack of them, bind a texture, or make an object depend on
 * the calls before them and are not counted as such. What looks at the state, or may change it without being followed, ends a run of calls that can be
 * taken together (a barrier): a call of a list that is made, a push or a pop of attributes, a query, a texture or a list that is deleted, a context made current.
 */
#ifndef STEREO_REPLAY_CONSISTENCYCHECKER_STATE_CENSUS_H
#define STEREO_REPLAY_CONSISTENCYCHECKER_STATE_CENSUS_H

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
        namespace ConsistencyChecker
        {
            class StateCensus : public ::Replay::ReplayEngineDLL::ICallInterceptor
            {
            public:
                /* What the calls of one function were in the dead frames. */
                struct FunctionEntry
                {
                    StateCallClass              call_class          = StateCallClass::OTHER;
                    APIInterceptor::APIFunction function            = APIInterceptor::APIFUNCTION_UNKNOWN;
                    uint64_t                    n_calls             = 0; /* all that were seen                                                                   */
                    uint64_t                    n_in_lists          = 0; /* of them, made into a list that is being made: they are kept in it                    */
                    uint64_t                    n_keyed             = 0; /* of them, that give a piece of state a value                                          */
                    uint64_t                    n_overwritten_later = 0; /* of the keyed, overwritten by another call before a frame that is made or a barrier */
                    uint64_t                    n_repeats           = 0; /* of the keyed, that give the value that the state has                                 */
                    uint64_t                    n_skipped_by_others = 0; /* of them, left out before this saw them made (the cache of texture parameters)        */
                };

                /* The frames by their numbers: those that are not live are dead; a frame past the end is live. */
                explicit StateCensus(const std::vector<bool>& in_is_live_vec);

                /* The frame that was last seen is done with. Called when the replay is over. */
                void finish();

                /* The frames whose calls change nothing that lasts: every push of attributes and of a matrix has its pop, and no call that sets state or a matrix
                 * comes outside of them, so that the state is what it was before the frame (but for what is bound and made, which is not told here).
                 */
                uint64_t get_n_neutral_frames() const;

                /* The dead frames that had calls; those of them whose pushes and pops of attributes and of matrices balance; and the calls outside the pushes. */
                uint64_t get_n_balanced_frames() const;
                uint64_t get_n_dead_frames_with_calls() const;
                uint64_t get_n_matrix_calls_outside_pushes() const;
                uint64_t get_n_state_calls_outside_pushes() const;

                /* The masks of the pushes of attributes in the dead frames, and how many of each. */
                const std::map<uint64_t, uint64_t>& get_push_mask_count_map() const;

                /* The functions seen in the dead frames, the one with the most calls first. */
                std::vector<FunctionEntry> get_entries() const;

                /* How often each kind of barrier came up in the dead frames, by the name of the function. */
                const std::map<std::string, uint64_t>& get_barrier_count_map() const;

                /* What has to be made when the runs are over, if the last value of each piece of state in a run is all that is made: the pieces that end a run with
                 * another value than they began it with (the others need nothing), counted for all of the runs. A value that is not known to be the same counts.
                 */
                uint64_t get_n_changed_keys() const;

                /* The runs of calls that can be taken together (those that a barrier or a frame that is made ends, that had a keyed call in them). */
                uint64_t get_n_runs() const;

                /* The pieces of state that runs end with the value they began with (the last call of each is not needed either). */
                uint64_t get_n_unchanged_keys() const;

                void on_call_replayed(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                      const uint32_t&                              in_n_frame,
                                      const bool&                                  in_was_executed) override;

                bool should_execute_call(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                         const uint32_t&                              in_n_frame) override;

            private:
                /* What a piece of state has been given, as far as the calls say: the numbers of the call, if they are all known. */
                struct StateValue
                {
                    bool                has_value = false;
                    std::vector<double> value_vec;
                };

                /* A piece of state that was given a value in the run: the function of the last call that did, and what it was before the run. */
                struct PendingEntry
                {
                    APIInterceptor::APIFunction function = APIInterceptor::APIFUNCTION_UNKNOWN;
                    StateValue                  value_before_run;
                };

                /* What a call is, as the census sees it. */
                struct ClassifiedCall
                {
                    StateCallClass call_class = StateCallClass::OTHER;
                    std::string    key;
                    StateValue     value;
                };

                /* The call as the census sees it: what it does to the state, the piece of state it gives a value, and the value. */
                ClassifiedCall classify(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                        const char*                                  in_name_ptr) const;

                /* The frame is done with: it counts as balanced or neutral, or not. */
                void end_frame();

                /* The calls that may have been taken together are over: the next that is made sees the state as it is. */
                void end_run();

                /* What the call, made in a dead frame, does to the pushes of attributes and of matrices, and to what is changed outside of them. */
                void follow_frame(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                  const char*                                  in_name_ptr,
                                  const ClassifiedCall&                        in_classified);

                uint32_t                                             m_active_unit;
                int64_t                                              m_attribute_depth;            /* of the frame that was last seen: the pushes of attributes not yet popped */
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
                uint64_t                                             m_n_runs_changed_keys;        /* the pieces of state that the runs ended with a value other than they began with */
                uint64_t                                             m_n_runs_unchanged_keys;      /* the same, with the value they began with                                  */
                uint64_t                                             m_n_state_calls_outside;
                std::map<std::string, PendingEntry>                  m_pending_map;                /* by piece of state, in the run                                             */
                std::map<uint64_t, uint64_t>                         m_push_mask_count_map;
                uint32_t                                             m_seen_frame;
                std::map<std::string, StateValue>                    m_state_map;                  /* the state as the calls so far have made it, by piece of state             */
                std::map<std::pair<uint32_t, uint32_t>, uint32_t>    m_texture_binding_map;        /* by unit and target: the texture                                           */
            };
        }
    }
}

#endif /* STEREO_REPLAY_CONSISTENCYCHECKER_STATE_CENSUS_H */
