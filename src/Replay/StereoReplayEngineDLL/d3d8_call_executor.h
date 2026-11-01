/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREOREPLAYENGINEDLL_D3D8_CALL_EXECUTOR_H
#define STEREOREPLAYENGINEDLL_D3D8_CALL_EXECUTOR_H

#include "Interception/Hooks/D3D8/types.h"
#include "d3d8_lock_replayer.h"
#include "d3d8_replay_context.h"
#include "i_call_executor.h"
#include "object_remapper.h"
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <Windows.h>

namespace Replay
{
    namespace StereoReplayEngineDLL
    {
        /* Replays the D3D8 calls on the d3d8.dll of the system. The objects are created by the replay, and the dump's
         * pointers to them are mapped to the live ones. Only a replay of the same pointer size as the dumped process can do
         * it (D3D8 is 32-bit).
         */
        class D3D8CallExecutor : public ICallExecutor
        {
        public:
            D3D8CallExecutor(ObjectRemapper* in_remapper_ptr);
            ~D3D8CallExecutor();

            bool           execute            (const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                               const IChunkSource&                          in_chunk_source,
                                               std::string*                                 out_error_ptr)         override;
            ExecutionClass get_execution_class(const Capture::APIDumpLoader::DumpedAPICall& in_call)         const override;
            void           on_frame_presented ()                                                                   override;
            bool           read_back_buffer   (std::vector<uint8_t>*                        out_pixel_u8_vec_ptr,
                                               FrameImage*                                  out_image_ptr)         override;
            void           reset              ()                                                                   override;
            void           restore_state      ()                                                                   override;
            void           save_state         ()                                                                   override;
            bool           write_back_buffer  (const std::vector<uint8_t>&                  in_pixel_u8_vec)       override;

        private:
            typedef bool (*InvokeFuncPtr)(D3D8ReplayContext* in_context_ptr,
                                          std::string*       out_error_ptr);

            /* The replay of a method, and how many arguments the method itself has (the dumper adds some). */
            struct MethodInvoker
            {
                InvokeFuncPtr invoke_func_ptr;
                size_t        n_head_arguments;
            };

            /* The back buffer copied into an image surface that is locked; what write_back_buffer() puts in it goes back. */
            struct BackBufferImage
            {
                void*                                      back_buffer_ptr = nullptr;
                Interception::Hooks::D3D8::D3DSURFACE_DESC description     = {};
                void*                                      image_ptr       = nullptr;
                bool                                       is_locked       = false;
                Interception::Hooks::D3D8::D3DLOCKED_RECT  locked_rect     = {};
            };

            void close_back_buffer_image (BackBufferImage*                             inout_image_ptr);
            bool create_direct3d8        (const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                          std::string*                                 out_error_ptr);
            bool open_back_buffer_image  (const bool&                                  in_copies_from_back_buffer,
                                          const DWORD&                                 in_lock_flags,
                                          BackBufferImage*                             out_image_ptr);
            bool release                 (const Capture::APIDumpLoader::DumpedAPICall& in_call);
            void release_scratch_surface ();
            void unlock_back_buffer_image(BackBufferImage*                             inout_image_ptr);

            /* The methods whose arguments hold data that the dumper does not store (the data of a private data call, what a
             * query writes through a pointer to memory of an unknown size): they are not replayed.
             */
            static bool is_ignored_method(const char* in_method_name_ptr);


            std::vector<uint8_t>                                            m_capture_u8_vec;
            D3D8ReplayContext                                               m_context;
            HMODULE                                                         m_d3d8_module;
            std::unordered_map<APIInterceptor::APIFunction, MethodInvoker>  m_invoker_map;
            D3D8LockReplayer                                                m_lock_replayer;
            std::unordered_set<APIInterceptor::APIFunction>                 m_release_function_set;
            Interception::Hooks::D3D8::D3DSURFACE_DESC                      m_scratch_description; /* of the back buffer which the scratch surface is for */
            void*                                                           m_scratch_device_ptr;
            void*                                                           m_scratch_surface_ptr; /* an image surface the size of the back buffer, kept for the next read */
            DWORD                                                           m_state_block_token;
        };
    }
}

#endif /* STEREOREPLAYENGINEDLL_D3D8_CALL_EXECUTOR_H */
