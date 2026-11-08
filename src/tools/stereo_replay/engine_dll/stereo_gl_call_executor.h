/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_REPLAY_ENGINEDLL_STEREO_GL_CALL_EXECUTOR_H
#define STEREO_REPLAY_ENGINEDLL_STEREO_GL_CALL_EXECUTOR_H

#include "gl_call_executor.h"
#include "i_eye_executor.h"
#include <map>

namespace Stereo
{
    namespace Replay
    {
        namespace EngineDLL
        {
            /* Replays the OpenGL calls for two eyes: a texture which each eye has a copy of (see PerEyeTexture: one that rendering fills and a frame reads what an earlier frame
             * left in) is two textures, one for each eye, and the calls of an eye use the copy of that eye. A texture that is not one of those is shared, and needs nothing:
             * a frame that is executed twice makes what it reads of it again in each execution.
             *
             * The copy is made when the texture is first changed after the analysis knows what it is (a texture has no resource before it is given a size): it takes the
             * parameters of the texture, and every call that changes the texture from then on is made for the copy too, in a frame that is executed once, since in one that
             * is executed twice each execution makes it for its own eye. The name that the program has for the texture is that of the eye's copy: what the remapper maps it to
             * changes with the eye, and so does what is bound.
             */
            class StereoGlCallExecutor : public ::Replay::ReplayEngineDLL::GlCallExecutor,
                                         public IEyeExecutor
            {
            public:
                explicit StereoGlCallExecutor(::Replay::ReplayEngineDLL::ObjectRemapper* in_remapper_ptr);

                bool execute              (const Capture::APIDumpLoader::DumpedAPICall&   in_call,
                                           const ::Replay::ReplayEngineDLL::IChunkSource& in_chunk_source,
                                           std::string*                                   out_error_ptr) override;
                void reset                ()                                                             override;
                void restore_state        ()                                                             override;
                void set_eye              (const Analysis::Recipe::Eye&                   in_eye)        override;
                void set_n_executions     (const uint32_t&                                in_n_executions) override;
                void set_per_eye_predicate(const PerEyePredicate&                         in_predicate)  override;

            private:
                /* The two copies of a texture that each eye has a copy of, by the names of GL. */
                struct TextureTwin
                {
                    uint32_t left_live_name  = 0;
                    uint32_t right_live_name = 0;
                };

                /* Puts what the eye's copies are into effect: what the remapper maps the name of the program to, and what each unit has bound. */
                void apply_eye();

                /* The texture is made, once the analysis knows it is one that each eye has a copy of: a texture with the parameters of the one that is bound, and the dumped
                 * name for it. False if GL does not give a name.
                 */
                bool make_twin(const uint32_t& in_dumped_name,
                               const uint32_t& in_live_name);

                /* The copy of the other eye of the texture that the call changes, which is made the same by making the call for it as well. */
                bool replicate_for_other_eye(const Capture::APIDumpLoader::DumpedAPICall&   in_call,
                                             const ::Replay::ReplayEngineDLL::IChunkSource& in_chunk_source,
                                             const uint32_t&                                in_dumped_name,
                                             std::string*                                   out_error_ptr);

                /* True if the call changes the texture that is bound to the target of the active unit (the pixels, the parameters, the levels). */
                static bool changes_bound_texture(const APIInterceptor::APIFunction& in_function);

                Analysis::Recipe::Eye                    m_eye;
                bool                                     m_is_replicating;   /* the call is being made again for the other eye */
                uint32_t                                 m_n_executions;     /* of the frame being played: 1 or 2              */
                PerEyePredicate                          m_per_eye_predicate;
                ::Replay::ReplayEngineDLL::ObjectRemapper* m_remapper_ptr;   /* the one that maps the names of the program to those of GL */
                std::map<uint32_t, TextureTwin>          m_twin_map;         /* by the dumped name of the texture */
            };
        }
    }
}

#endif /* STEREO_REPLAY_ENGINEDLL_STEREO_GL_CALL_EXECUTOR_H */
