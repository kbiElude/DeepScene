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
            /* Leaves out glTexParameteri/f calls that set a parameter to its current value. Only parameters the twin texture of the other
             * eye also receives are tracked (see get_cached_parameters()). The cache forgets everything when a call may change its
             * knowledge unseen (texture-state list call, texture deletion, glPopAttrib, context create/delete, a call another interceptor
             * left out, replay restart). A texture's first parameter call is never left out.
             */
            class GLTextureParameterCache : public ::Replay::ReplayEngineDLL::ICallInterceptor
            {
            public:
                GLTextureParameterCache();

                /* OpenGL, and WGL for the contexts. */
                std::vector<Capture::APIDumpLoader::APIGroup> get_api_groups() const override;

                /* Calls left out so far. */
                uint64_t get_n_skipped_calls() const;

                void on_call_replayed(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                      const uint32_t&                              in_n_frame,
                                      const bool&                                  in_was_executed) override;

                bool should_execute_call(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                         const uint32_t&                              in_n_frame) override;

                /* Tracked parameters: filters, wrap modes, mipmap level range and LOD, mipmap generation. */
                static const std::vector<unsigned int>& get_cached_parameters();

            private:
                /* Texture unit and target. */
                typedef std::pair<uint32_t, uint32_t> BindingKey;

                /* Target, texture name, parameter. */
                typedef std::tuple<uint32_t, uint32_t, uint32_t> ParameterKey;

                /* Texture bound to the target on the active unit; false if unknown. */
                bool find_bound_texture(const uint32_t& in_target,
                                        uint32_t*       out_texture_ptr) const;

                /* Forgets all texture knowledge; known lists are kept (a list's effect never changes). */
                void forget_all();

                /* Not used for calls recorded into a list. */
                void update_for_executed_call(const Capture::APIDumpLoader::DumpedAPICall& in_call);

                static bool is_cached_parameter(const uint32_t& in_parameter);

                /* Whether the call may change the bound texture or a parameter (inside a list: what the list does). */
                static bool is_texture_state_call(const APIInterceptor::APIFunction& in_function);

                /* Reads a glTexParameteri/f call; false for another function or an unreadable argument. */
                static bool read_scalar_parameter_call(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                       uint32_t*                                    out_parameter_ptr,
                                                       uint32_t*                                    out_target_ptr,
                                                       double*                                      out_value_ptr);

                uint32_t                       m_active_unit;
                std::map<BindingKey, uint32_t> m_bound_texture_map;
                bool                           m_is_recording;
                bool                           m_is_unit_known;                /* m_active_unit matches the context  */
                std::set<uint32_t>             m_known_list_set;               /* lists made, by number             */
                uint64_t                       m_last_context;                 /* last context made current (not 0) */
                uint32_t                       m_last_frame;
                uint64_t                       m_n_skipped_calls;
                uint32_t                       m_recording_list;
                bool                           m_recording_sets_texture_state;
                bool                           m_skipped_last_call;            /* left out the call on_call_replayed() reports next */
                std::set<uint32_t>             m_texture_list_set;             /* lists that set or may set texture state           */
                std::map<ParameterKey, double> m_value_map;
            };
        }
    }
}

#endif /* STEREO_REPLAY_ENGINEDLL_GL_TEXTURE_PARAMETER_CACHE_H */
