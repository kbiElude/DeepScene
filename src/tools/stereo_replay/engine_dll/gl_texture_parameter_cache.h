/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_REPLAY_ENGINEDLL_GL_TEXTURE_PARAMETER_CACHE_H
#define STEREO_REPLAY_ENGINEDLL_GL_TEXTURE_PARAMETER_CACHE_H

#include "capture/api_dump_loader/api_dump_loader.h"
#include "replay/replay_engine_dll/i_call_interceptor.h"
#include <map>
#include <set>
#include <stdint.h>
#include <tuple>
#include <utility>
#include <vector>

namespace Stereo
{
    namespace Replay
    {
        namespace EngineDLL
        {
            /* Leaves out the glTexParameteri and glTexParameterf calls that give a parameter of a texture the value it has: a driver takes a call of the kind
             * the same time whether it changes anything or not, and a workload sets the same few parameters again and again (Gerbera: 465,000 calls in the
             * 7,000 frames that a seek to one of its later frames makes). Only the parameters that a texture's twin for the other eye takes
             * (see get_cached_parameters()) are looked at, so that a call that is left out is one the twin would not have changed either.
             *
             * What the cache knows is forgotten (and not guessed) whenever a call may change it without it being told: a list that sets the texture state, called,
             * the deletion of textures, glPopAttrib, a context made or deleted, a call that another interceptor left out, and a replay that starts over. The first
             * call that gives a parameter of a texture its value is never left out, as the value that it has until then is not known.
             */
            class GlTextureParameterCache : public ::Replay::ReplayEngineDLL::ICallInterceptor
            {
            public:
                GlTextureParameterCache();

                /* The calls that were left out since the cache was made. */
                uint64_t get_n_skipped_calls() const;

                void on_call_replayed(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                      const uint32_t&                              in_n_frame,
                                      const bool&                                  in_was_executed) override;

                bool should_execute_call(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                         const uint32_t&                              in_n_frame) override;

                /* The parameters that are looked at: the filters, how the coordinates wrap, the levels that are used and the generation of levels. */
                static const std::vector<unsigned int>& get_cached_parameters();

            private:
                /* A texture unit and a target that a texture is bound to. */
                typedef std::pair<uint32_t, uint32_t> BindingKey;

                /* The target of a texture, its name and a parameter of it. */
                typedef std::tuple<uint32_t, uint32_t, uint32_t> ParameterKey;

                /* The texture that is bound to the target on the active unit; false if it is not known. */
                bool find_bound_texture(const uint32_t& in_target,
                                        uint32_t*       out_texture_ptr) const;

                /* Nothing is known of the textures from now on. The lists that are known are kept: what they do does not change. */
                void forget_all();

                /* A call that was made, for what it does to what is known; not in a list that is being made. */
                void update_for_executed_call(const Capture::APIDumpLoader::DumpedAPICall& in_call);

                static bool is_cached_parameter(const uint32_t& in_parameter);

                /* Whether the call may change what is bound, a texture's parameters, or what a call of a list that has it will change. */
                static bool is_texture_state_call(const APIInterceptor::APIFunction& in_function);

                /* The target, the parameter and the value of a glTexParameteri or glTexParameterf; false if it is another call, or has what cannot be read. */
                static bool read_scalar_parameter_call(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                       uint32_t*                                    out_parameter_ptr,
                                                       uint32_t*                                    out_target_ptr,
                                                       double*                                      out_value_ptr);

                uint32_t                       m_active_unit;
                std::map<BindingKey, uint32_t> m_bound_texture_map;
                bool                           m_is_recording;
                bool                           m_is_unit_known;                /* whether m_active_unit is what the context has                              */
                std::set<uint32_t>             m_known_list_set;               /* the lists that were made, by number                                        */
                uint64_t                       m_last_context;                 /* the last context made current that was not none                            */
                uint32_t                       m_last_frame;
                uint64_t                       m_n_skipped_calls;
                uint32_t                       m_recording_list;
                bool                           m_recording_sets_texture_state;
                bool                           m_skipped_last_call;            /* this cache left out the call that on_call_replayed() is about to be told of */
                std::set<uint32_t>             m_texture_list_set;             /* the lists that set the texture state, or may                               */
                std::map<ParameterKey, double> m_value_map;
            };
        }
    }
}

#endif /* STEREO_REPLAY_ENGINEDLL_GL_TEXTURE_PARAMETER_CACHE_H */
