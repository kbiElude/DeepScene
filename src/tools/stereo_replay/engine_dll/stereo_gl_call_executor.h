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
            /* Replays the OpenGL calls for two eyes. Some textures have a copy for each eye (see PerEyeTexture): rendering fills them,
             * and a later frame reads what an earlier frame left in them. Each such texture is two GL textures, and the calls of an eye
             * use the copy of that eye. Other textures are shared and need nothing, because a frame that is executed twice recreates
             * what it reads from them in each execution.
             *
             * The copy is created when the texture is first changed after the analysis knows that the texture has a copy per eye (a
             * texture has no storage before it is given a size). The copy takes over the parameters of the texture. From then on every
             * call that changes the texture also runs for the copy, in a frame that is executed once. In a frame that is executed twice,
             * each execution changes the texture of its own eye. The name that the program uses for the texture refers to the copy of
             * the current eye: the remapper maps that name to a different GL name for each eye, and so does the texture binding.
             */
            class StereoGLCallExecutor : public ::Replay::ReplayEngineDLL::GLCallExecutor,
                                         public IEyeExecutor
            {
            public:
                explicit StereoGLCallExecutor(::Replay::ReplayEngineDLL::ObjectRemapper* in_remapper_ptr);

                bool execute              (const Capture::APIDumpLoader::DumpedAPICall&   in_call,
                                           const ::Replay::ReplayEngineDLL::IChunkSource& in_chunk_source,
                                           std::string*                                   out_error_ptr) override;
                void reset                ()                                                               override;
                void restore_state        ()                                                               override;
                void set_eye              (const Analysis::Recipe::Eye&                   in_eye)          override;
                void set_n_executions     (const uint32_t&                                in_n_executions) override;
                void set_per_eye_predicate(const PerEyePredicate&                         in_predicate)    override;

            private:
                typedef void         (APIENTRY* PFNACTIVETEXTUREPROC)  (unsigned int);
                typedef void         (APIENTRY* PFNBINDTEXTUREPROC)    (unsigned int, unsigned int);
                typedef void         (APIENTRY* PFNDELETETEXTURESPROC) (int, const unsigned int*);
                typedef unsigned int (APIENTRY* PFNGETERRORPROC)       ();
                typedef void         (APIENTRY* PFNGENTEXTURESPROC)    (int, unsigned int*);
                typedef void         (APIENTRY* PFNGETINTEGERVPROC)    (unsigned int, int*);
                typedef void         (APIENTRY* PFNGETTEXPARAMETERIVPROC)(unsigned int, unsigned int, int*);
                typedef void         (APIENTRY* PFNTEXPARAMETERIPROC)  (unsigned int, unsigned int, int);

                /* The two copies of a texture that has a copy per eye, identified by their GL names. */
                struct TextureTwin
                {
                    uint32_t left_live_name  = 0;
                    uint32_t right_live_name = 0;
                };

                /* Puts the copies of the current eye into effect: the remapper maps the program's names to them, and each texture unit
                 * gets the copy bound.
                 */
                void apply_eye();

                /* Creates the copy of a texture once the analysis knows the texture has a copy per eye. The copy is a new GL texture with
                 * the parameters of the bound texture, stored under the dumped name of the texture. Returns false if GL does not return
                 * a name.
                 */
                bool make_twin(const uint32_t& in_dumped_name,
                               const uint32_t& in_live_name);

                /* Runs the call again on the other eye's copy of the texture that the call changes, so that both copies stay the same. */
                bool replicate_for_other_eye(const Capture::APIDumpLoader::DumpedAPICall&   in_call,
                                             const ::Replay::ReplayEngineDLL::IChunkSource& in_chunk_source,
                                             const uint32_t&                                in_dumped_name,
                                             std::string*                                   out_error_ptr);

                /* True if the call changes the texture bound to the target of the active unit (its pixels, parameters or levels). */
                static bool changes_bound_texture(const APIInterceptor::APIFunction& in_function);

                static const unsigned int TEXTURE_2D;
                static const unsigned int TEXTURE_BINDING_2D;

                Analysis::Recipe::Eye                    m_eye;
                bool                                     m_is_replicating;   /* the call is being run again for the other eye */
                uint32_t                                 m_n_executions;     /* executions of the frame being played: 1 or 2   */
                PerEyePredicate                          m_per_eye_predicate;
                ::Replay::ReplayEngineDLL::ObjectRemapper* m_remapper_ptr;   /* maps the names of the program to GL names */
                std::map<uint32_t, TextureTwin>          m_twin_map;         /* by the dumped name of the texture */
            };
        }
    }
}

#endif /* STEREO_REPLAY_ENGINEDLL_STEREO_GL_CALL_EXECUTOR_H */
