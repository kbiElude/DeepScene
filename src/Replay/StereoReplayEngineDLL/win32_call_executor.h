/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREOREPLAYENGINEDLL_WIN32_CALL_EXECUTOR_H
#define STEREOREPLAYENGINEDLL_WIN32_CALL_EXECUTOR_H

#include "i_call_executor.h"
#include "object_remapper.h"
#include "Replay/StereoReplayEngineDLL/types.h"
#include <functional>
#include <map>
#include <set>
#include <string>
#include <Windows.h>

namespace Replay
{
    namespace StereoReplayEngineDLL
    {
        /* Replays the windowing calls (USER32, GDI32) and the WGL ones, on the replay thread. The windows are created from
         * scratch: the window classes the app registered are not part of the dump, so a class of the same name is
         * registered here. Calls which only ask something (e.g. GetPixelFormat), or which the driver made on its own
         * (e.g. GetDC of no window), are not replayed: the driver makes them again.
         */
        class Win32CallExecutor : public ICallExecutor
        {
        public:
            /* Called, from the replay thread, when the replay has put one of its windows in place. */
            typedef std::function<void(const WindowPlacement&)> WindowPlacedFunc;

            Win32CallExecutor(ObjectRemapper*         in_remapper_ptr,
                              const WindowPlacedFunc& in_window_placed_func);
            ~Win32CallExecutor();

            bool execute           (const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                             const CallWalker&                   in_walker,
                                             std::string*                        out_error_ptr) override;
            void on_frame_presented() override;
            void on_idle           () override;
            void reset             () override;
            void shutdown          () override;

        private:
            /* What makes a window of the app the one to use again after a reset. */
            struct WindowSignature
            {
                std::wstring class_name;
                DWORD        ex_style = 0;
                int          height   = 0;
                DWORD        style    = 0;
                std::wstring title;
                int          width    = 0;
            };

            typedef bool (Win32CallExecutor::*HandlerFuncPtr)(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                       const CallWalker&                   in_walker,
                                                                       std::string*                        out_error_ptr);

            bool  create_window_ex             (const Capture::APIDumpLoader::DumpedAPICall&        in_call,
                                                         const CallWalker&                          in_walker,
                                                         const bool&                                in_is_wide,
                                                         std::string*                               out_error_ptr);
            void  destroy_parked_windows       ();
            bool  get_live_handle              (const ObjectKind&                          in_kind,
                                                const APIInterceptor::APIFunctionArgument& in_argument,
                                                void**                                     out_live_handle_ptr,
                                                std::string*                               out_error_ptr) const;
            void* get_wgl_function_ptr         (const HDC&                                 in_hdc,
                                                const char*                                in_name_ptr) const;
            void  make_non_scalable            (const DWORD&                               in_ex_style,
                                                DWORD*                                     inout_style_ptr,
                                                int*                                       inout_width_ptr,
                                                int*                                       inout_height_ptr);
            bool  on_copy_context              (const Capture::APIDumpLoader::DumpedAPICall&        in_call,
                                                         const CallWalker&                          in_walker,
                                                         std::string*                               out_error_ptr);
            bool  on_create_context            (const Capture::APIDumpLoader::DumpedAPICall&        in_call,
                                                         const CallWalker&                          in_walker,
                                                         std::string*                               out_error_ptr);
            bool  on_create_context_attribs_arb(const Capture::APIDumpLoader::DumpedAPICall&        in_call,
                                                         const CallWalker&                          in_walker,
                                                         std::string*                               out_error_ptr);
            bool  on_create_layer_context      (const Capture::APIDumpLoader::DumpedAPICall&        in_call,
                                                         const CallWalker&                          in_walker,
                                                         std::string*                               out_error_ptr);
            bool  on_create_window_ex_a        (const Capture::APIDumpLoader::DumpedAPICall&        in_call,
                                                         const CallWalker&                          in_walker,
                                                         std::string*                               out_error_ptr);
            bool  on_create_window_ex_w        (const Capture::APIDumpLoader::DumpedAPICall&        in_call,
                                                         const CallWalker&                          in_walker,
                                                         std::string*                               out_error_ptr);
            bool  on_delete_context            (const Capture::APIDumpLoader::DumpedAPICall&        in_call,
                                                         const CallWalker&                          in_walker,
                                                         std::string*                               out_error_ptr);
            bool  on_get_dc                    (const Capture::APIDumpLoader::DumpedAPICall&        in_call,
                                                         const CallWalker&                          in_walker,
                                                         std::string*                               out_error_ptr);
            bool  on_ignored_call              (const Capture::APIDumpLoader::DumpedAPICall&        in_call,
                                                         const CallWalker&                          in_walker,
                                                         std::string*                               out_error_ptr);
            bool  on_make_current              (const Capture::APIDumpLoader::DumpedAPICall&        in_call,
                                                         const CallWalker&                          in_walker,
                                                         std::string*                               out_error_ptr);
            bool  on_release_dc                (const Capture::APIDumpLoader::DumpedAPICall&        in_call,
                                                         const CallWalker&                          in_walker,
                                                         std::string*                               out_error_ptr);
            bool  on_set_pixel_format          (const Capture::APIDumpLoader::DumpedAPICall&        in_call,
                                                         const CallWalker&                          in_walker,
                                                         std::string*                               out_error_ptr);
            bool  on_share_lists               (const Capture::APIDumpLoader::DumpedAPICall&        in_call,
                                                         const CallWalker&                          in_walker,
                                                         std::string*                               out_error_ptr);
            bool  on_swap_buffers              (const Capture::APIDumpLoader::DumpedAPICall&        in_call,
                                                         const CallWalker&                          in_walker,
                                                         std::string*                               out_error_ptr);
            bool  on_swap_interval_ext         (const Capture::APIDumpLoader::DumpedAPICall&        in_call,
                                                         const CallWalker&                          in_walker,
                                                         std::string*                               out_error_ptr);
            bool  on_swap_layer_buffers        (const Capture::APIDumpLoader::DumpedAPICall&        in_call,
                                                         const CallWalker&                          in_walker,
                                                         std::string*                               out_error_ptr);
            void  place_window                 (const HWND&                                in_window);
            void  pump_messages                ();
            bool  register_window_class        (const std::wstring&                        in_name);
            bool  take_parked_window           (const WindowSignature&                     in_signature,
                                                HWND*                                      out_window_ptr);

            static std::wstring     get_wide_string(const Capture::APIDumpLoader::DataChunk& in_chunk,
                                                             const bool&                     in_is_wide);
            static LRESULT CALLBACK window_proc    (HWND                            in_hwnd,
                                                    UINT                            in_message,
                                                    WPARAM                          in_wparam,
                                                    LPARAM                          in_lparam);

            std::set<HGLRC>                              m_context_set;
            std::map<APIInterceptor::APIFunction, HandlerFuncPtr> m_handler_func_ptr_map;
            bool                                         m_has_last_placement;
            WindowPlacement                              m_last_placement;
            std::map<HWND, WindowSignature>              m_parked_window_map;
            std::set<std::wstring>                       m_registered_class_name_set;
            ObjectRemapper*                              m_remapper_ptr;
            WindowPlacedFunc                             m_window_placed_func;
            std::map<HWND, WindowSignature>              m_window_signature_map;
        };
    }
}

#endif /* STEREOREPLAYENGINEDLL_WIN32_CALL_EXECUTOR_H */
