/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREOREPLAYENGINEDLL_GL_CALL_EXECUTOR_H
#define STEREOREPLAYENGINEDLL_GL_CALL_EXECUTOR_H

#include "gl_client_array_replayer.h"
#include "gl_index_replayer.h"
#include "gl_mapping_replayer.h"
#include "gl_replay_context.h"
#include "i_call_executor.h"
#include "object_remapper.h"
#include <map>
#include <string>
#include <Windows.h>

namespace Replay
{
    namespace StereoReplayEngineDLL
    {
        /* Replays the GL calls on the GL context that the WGL calls of the dump made current. The functions are the ones of
         * the system's driver; the names of the objects (textures, buffers...) and the locations of uniforms are mapped to
         * what the driver hands out to the replay.
         */
        class GlCallExecutor : public ICallExecutor
        {
        public:
             GlCallExecutor(ObjectRemapper* in_remapper_ptr);
            ~GlCallExecutor();

            bool execute           (const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                    const IChunkSource&                          in_chunk_source,
                                    std::string*                                 out_error_ptr) override;
            void on_frame_presented() override;
            void reset             () override;

        private:
            typedef bool (*InvokeFuncPtr)(GlReplayContext* in_context_ptr,
                                          void*            in_function_ptr,
                                          std::string*     out_error_ptr);

            /* A function of the table, and the way to replay it. */
            struct FunctionEntry
            {
                GlFunctionInfo info;
                InvokeFuncPtr  invoke_func_ptr;
                size_t         n_head_arguments;
            };

            void  add_function           (const APIInterceptor::APIFunction&           in_function,
                                          const char*                                  in_name_ptr,
                                          const ObjectKind&                            in_return_kind,
                                          const std::vector<ObjectKind>&               in_param_kind_vec,
                                          const uint32_t&                              in_ignored_param_mask,
                                          const InvokeFuncPtr&                         in_invoke_func_ptr,
                                          const size_t&                                in_n_head_arguments);
            void  after_call             (const Capture::APIDumpLoader::DumpedAPICall& in_call);
            void* get_proc_ptr           (const char*                                  in_name_ptr);
            void  register_returned_index(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                          const ObjectKind&                            in_kind,
                                          const bool&                                  in_has_scope);

            /* wglGetProcAddress() says a function is not there by one of these. */
            static bool is_valid_proc(const void* in_proc_ptr);

            GlClientArrayReplayer                                m_client_array_replayer;
            GlReplayContext                                      m_context;
            GlIndexReplayer                                      m_index_replayer;
            std::map<APIInterceptor::APIFunction, FunctionEntry> m_entry_map;
            GlMappingReplayer                                    m_mapping_replayer;
            HMODULE                                              m_opengl32_module;
            std::map<std::string, void*>                         m_proc_ptr_map;
        };
    }
}

#endif /* STEREOREPLAYENGINEDLL_GL_CALL_EXECUTOR_H */
