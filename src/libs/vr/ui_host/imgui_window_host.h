/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef UIHOST_IMGUI_WINDOW_HOST_H
#define UIHOST_IMGUI_WINDOW_HOST_H

#include "vr/ui_host/types.h"
#include <string>
#include <Windows.h>

struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11RenderTargetView;
struct IDXGISwapChain;

namespace VR
{
    namespace UIHost
    {
        /* A window which is drawn with ImGui over a Direct3D 11 device of its own: the device, the swap chain, the message loop and
         * the scale of the monitor are here, and what the window shows is in the class which derives from this one. It lives on the
         * thread which runs it.
         */
        class ImGuiWindowHost
        {
        public:
            explicit ImGuiWindowHost (const WindowOptions& in_options);
            virtual  ~ImGuiWindowHost();

            /* Creates the window and the device. Returns false, with the reason, if that fails. */
            bool create(std::string* out_error_ptr);

            HWND get_window() const;

            /* Asks the window to close, as if the user had: run() returns. Any thread. */
            void request_close();

            /* Shows the window and returns when the user closes it. */
            void run();

            /* Process-wide; before any window is made, so that sizes are in pixels. */
            static void enable_dpi_awareness();

        protected:
            /* Draws what the window shows, in the window which fills it. The extent is to be extended with every item, so that a window
             * which fits its content knows how big it has to be (see extend_content()).
             */
            virtual void draw_contents(ContentExtent* inout_extent_ptr) = 0;

            /* The device of the window, for what a derived window makes with it (textures). */
            ID3D11DeviceContext* get_device_context_ptr() const;
            ID3D11Device*        get_device_ptr        () const;

            /* Returns true, with the result, if the message is dealt with; ImGui has seen it already. */
            virtual bool handle_message(const HWND&   in_window,
                                        const UINT&   in_message,
                                        const WPARAM& in_wparam,
                                        const LPARAM& in_lparam,
                                        LRESULT*      out_result_ptr);

            /* The window has been made fit what it shows. */
            virtual void on_resized_to_content();

            /* Takes in the last item which was drawn. */
            static void extend_content(ContentExtent* inout_extent_ptr);

        private:
            bool    create_device        ();
            void    create_render_target ();
            void    destroy_render_target();
            void    fit_to_content       ();
            LRESULT process_message      (const HWND&   in_window,
                                          const UINT&   in_message,
                                          const WPARAM& in_wparam,
                                          const LPARAM& in_lparam);
            void    render_frame         ();
            void    resize               (const UINT&   in_width,
                                          const UINT&   in_height);

            static LRESULT CALLBACK window_proc(HWND   in_window,
                                                UINT   in_message,
                                                WPARAM in_wparam,
                                                LPARAM in_lparam);

            int                     m_content_height;
            int                     m_content_width;
            ID3D11DeviceContext*    m_device_context_ptr;
            ID3D11Device*           m_device_ptr;
            HWND                    m_hwnd;
            bool                    m_is_imgui_ready;
            WindowOptions           m_options;
            ID3D11RenderTargetView* m_render_target_view_ptr;
            IDXGISwapChain*         m_swap_chain_ptr;
        };
    }
}

#endif /* UIHOST_IMGUI_WINDOW_HOST_H */
