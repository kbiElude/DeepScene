/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_REPLAY_ENGINEDLL_STEREO_D3D7_CALL_EXECUTOR_H
#define STEREO_REPLAY_ENGINEDLL_STEREO_D3D7_CALL_EXECUTOR_H

#include "d3d7_call_executor.h"
#include "i_eye_executor.h"
#include <map>

namespace Stereo
{
    namespace Replay
    {
        namespace EngineDLL
        {
            /* Replays the Direct3D 7 calls for two eyes: an object which each eye has a copy of (a render target, a texture) is made twice, once
             * for each eye, by the call that makes it and by the calls that set it up, and the calls of an eye use the copy of that eye.
             */
            class StereoD3D7CallExecutor : public ::Replay::ReplayEngineDLL::D3D7CallExecutor,
                                           public IEyeExecutor
            {
            public:
                StereoD3D7CallExecutor(::Replay::ReplayEngineDLL::ObjectRemapper* in_remapper_ptr);

                void set_eye              (const Analysis::RecipeLoader::Eye& in_eye)       override;
                void set_per_eye_predicate(const PerEyePredicate&             in_predicate) override;

            protected:
                bool  after_call          (const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                           std::string*                                 out_error_ptr) override;
                void  on_object_gone      (void*                                        in_object_ptr) override;
                void  on_object_registered(const uint64_t&                              in_dumped_value,
                                           void*                                        in_live_object_ptr) override;
                void  on_object_released  (void*                                        in_object_ptr) override;
                void  on_reset            ()                                                           override;
                void* resolve_object      (void*                                        in_object_ptr) const override;
                bool  take_object         (const uint64_t&                              in_dumped_value,
                                           void*                                        in_live_object_ptr) override;

            private:
                /* The right eye's copy of the object, if the object is one that each eye has a copy of; nullptr if not. */
                void* find_right_twin(void* in_left_object_ptr) const;

                /* True if an object of the call has a copy for each eye: a call which sets such an object up is made for both. */
                bool involves_per_eye_object() const;

                Analysis::RecipeLoader::Eye m_eye;
                bool                        m_is_building_twin; /* the call is made for the right eye's copies */
                bool                        m_needs_twin;       /* the call made an object which each eye has a copy of */
                PerEyePredicate             m_per_eye_predicate;
                std::map<void*, void*>      m_right_twin_map;   /* the copy of each eye's object: by the left one */
            };
        }
    }
}

#endif /* STEREO_REPLAY_ENGINEDLL_STEREO_D3D7_CALL_EXECUTOR_H */
