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
            /* Leaves out glTexParameteri and glTexParameterf calls that set a texture parameter to the value it already has. A driver
             * takes the same time for such a call whether or not it changes anything, and workloads set the same few parameters again
             * and again (465,000 calls in the 7,000 frames that a seek to a late frame of one OpenGL demo plays). Only parameters that
             * the twin texture for the other eye also receives are tracked (see get_cached_parameters()), so a call that is left out is
             * one the twin would not have changed either.
             *
             * The cache forgets what it knows, instead of guessing, whenever a call may change that knowledge without the cache seeing
             * it: a call to a list that sets texture state, a texture deletion, glPopAttrib, a context that is created or deleted, a
             * call that another interceptor left out, and a replay that starts over. The first call that sets a parameter of a texture is
             * never left out, because the value is unknown until then.
             */
            class GLTextureParameterCache : public ::Replay::ReplayEngineDLL::ICallInterceptor
            {
            public:
                GLTextureParameterCache();

                /* OpenGL, and WGL for the contexts. */
                std::vector<Capture::APIDumpLoader::APIGroup> get_api_groups() const override;

                /* The number of calls left out since the cache was created. */
                uint64_t get_n_skipped_calls() const;

                void on_call_replayed(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                      const uint32_t&                              in_n_frame,
                                      const bool&                                  in_was_executed) override;

                bool should_execute_call(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                         const uint32_t&                              in_n_frame) override;

                /* The parameters that are tracked: the filters, the wrap modes, the mipmap level range (base and max level, min and max
                 * LOD) and mipmap generation.
                 */
                static const std::vector<unsigned int>& get_cached_parameters();

            private:
                /* A texture unit and a target; identifies where a texture is bound. */
                typedef std::pair<uint32_t, uint32_t> BindingKey;

                /* The target of a texture, the texture name and a parameter of that texture. */
                typedef std::tuple<uint32_t, uint32_t, uint32_t> ParameterKey;

                /* Finds the texture bound to the target on the active unit. Returns false if it is not known. */
                bool find_bound_texture(const uint32_t& in_target,
                                        uint32_t*       out_texture_ptr) const;

                /* Forgets everything known about textures. The known lists are kept because what a list does never changes. */
                void forget_all();

                /* Updates the cache for a call that was executed. It is not used for calls inside a list that is being recorded. */
                void update_for_executed_call(const Capture::APIDumpLoader::DumpedAPICall& in_call);

                static bool is_cached_parameter(const uint32_t& in_parameter);

                /* Whether the call may change the bound texture or a texture parameter. Inside a list, such a call changes what calling
                 * the list does.
                 */
                static bool is_texture_state_call(const APIInterceptor::APIFunction& in_function);

                /* Reads the target, parameter and value of a glTexParameteri or glTexParameterf call. Returns false if the call is another
                 * function or an argument cannot be read.
                 */
                static bool read_scalar_parameter_call(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                       uint32_t*                                    out_parameter_ptr,
                                                       uint32_t*                                    out_target_ptr,
                                                       double*                                      out_value_ptr);

                uint32_t                       m_active_unit;
                std::map<BindingKey, uint32_t> m_bound_texture_map;
                bool                           m_is_recording;
                bool                           m_is_unit_known;                /* whether m_active_unit is what the context has                              */
                std::set<uint32_t>             m_known_list_set;               /* the lists that were made, by number                                        */
                uint64_t                       m_last_context;                 /* the last context that was made current, ignoring none                      */
                uint32_t                       m_last_frame;
                uint64_t                       m_n_skipped_calls;
                uint32_t                       m_recording_list;
                bool                           m_recording_sets_texture_state;
                bool                           m_skipped_last_call;            /* this cache left out the call that on_call_replayed() reports next           */
                std::set<uint32_t>             m_texture_list_set;             /* the lists that set texture state, or may set it                            */
                std::map<ParameterKey, double> m_value_map;
            };
        }
    }
}

#endif /* STEREO_REPLAY_ENGINEDLL_GL_TEXTURE_PARAMETER_CACHE_H */
