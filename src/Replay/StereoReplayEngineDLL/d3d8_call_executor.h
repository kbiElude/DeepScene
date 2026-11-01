/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREOREPLAYENGINEDLL_D3D8_CALL_EXECUTOR_H
#define STEREOREPLAYENGINEDLL_D3D8_CALL_EXECUTOR_H

#include "d3d8_lock_replayer.h"
#include "d3d8_replay_context.h"
#include "i_call_executor.h"
#include "object_remapper.h"
#include <map>
#include <set>
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

            bool execute           (const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                             const CallWalker&                   in_walker,
                                             std::string*                        out_error_ptr) override;
            void on_frame_presented() override;
            void reset             () override;

        private:
            typedef bool (*InvokeFuncPtr)(D3D8ReplayContext* in_context_ptr,
                                          std::string*       out_error_ptr);

            /* The replay of a method, and how many arguments the method itself has (the dumper adds some). */
            struct MethodInvoker
            {
                InvokeFuncPtr invoke_func_ptr;
                size_t        n_head_arguments;
            };

            bool create_direct3d8(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                           std::string*                        out_error_ptr);
            bool release         (const Capture::APIDumpLoader::DumpedAPICall& in_call);

            /* The methods whose arguments hold data that the dumper does not store (the data of a private data call, what a
             * query writes through a pointer to memory of an unknown size): they are not replayed.
             */
            static bool is_ignored_method(const char* in_method_name_ptr);

            D3D8ReplayContext                                     m_context;
            HMODULE                                               m_d3d8_module;
            std::map<APIInterceptor::APIFunction, MethodInvoker>  m_invoker_map;
            D3D8LockReplayer                                      m_lock_replayer;
            std::set<APIInterceptor::APIFunction>                 m_release_function_set;
        };
    }
}

#endif /* STEREOREPLAYENGINEDLL_D3D8_CALL_EXECUTOR_H */
