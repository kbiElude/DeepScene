/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef UTILS_UI_IMGUI_PLAYER_WINDOW_BASE_H
#define UTILS_UI_IMGUI_PLAYER_WINDOW_BASE_H

#include "utils/ui/types.h"
#include <string>
#include <Windows.h>

struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11RenderTargetView;
struct IDXGISwapChain;

namespace Utils
{
    namespace UI
    {
        /* A window drawn with ImGui on its own Direct3D 11 device. This class owns the device, the swap chain, the message loop and the monitor
         * scale. What the window shows is implemented by the derived class. The window lives on the thread that runs it.
         */
        class ImGuiPlayerWindowBase
        {
        public:
            explicit ImGuiPlayerWindowBase (const WindowOptions& in_options);
            virtual  ~ImGuiPlayerWindowBase();

            /* Creates the window and the device. Returns false and sets the error on failure. */
            bool create(std::string* out_error_ptr);

            HWND get_window() const;

            /* Asks the window to close, as if the user had closed it. run() then returns. Callable from any thread. */
            void request_close();

            /* Shows the window and returns when the user closes it. */
            void run();

            /* A process-wide setting. Call it before any window is created so that sizes are in pixels. */
            static void enable_dpi_awareness();

        protected:
            /* Draws the contents of the window, which fill it. Implementations must extend the extent with every item they draw, so that a window
             * that fits its content knows how large it has to be (see extend_content()).
             */
            virtual void draw_contents(ContentExtent* inout_extent_ptr) = 0;

            /* The device of the window, for derived windows that create resources with it (such as textures). */
            ID3D11DeviceContext* get_device_context_ptr() const;
            ID3D11Device*        get_device_ptr        () const;

            /* The size of the window, frame included, that has a client area of the given size. With a custom title bar the window is all client area. */
            SIZE get_outer_size(const int& in_client_width,
                                const int& in_client_height) const;

            bool is_window_maximized() const;

            /* For a window with a custom title bar. */
            void minimize_window();

            /* For a window with a custom title bar: where it is, in the pixels of the window, so that the system can move the window by it. The bar is
             * everything above in_caption_height; the buttons (which the window draws and handles itself) are right of in_buttons_left and above
             * in_buttons_bottom. Call it from draw_contents.
             */
            void set_title_bar_layout(const int& in_caption_height,
                                      const int& in_buttons_left,
                                      const int& in_buttons_bottom);

            /* For a window with a custom title bar: maximizes the window, or restores it if it is maximized. */
            void toggle_maximize();

            /* Returns true and sets the result if the message was handled. ImGui has already seen the message. */
            virtual bool handle_message(const HWND&   in_window,
                                        const UINT&   in_message,
                                        const WPARAM& in_wparam,
                                        const LPARAM& in_lparam,
                                        LRESULT*      out_result_ptr);

            /* Called after the window has been resized to fit its content. */
            virtual void on_resized_to_content();

            /* Extends the extent to include the item that was drawn last. */
            static void extend_content(ContentExtent* inout_extent_ptr);

        private:
            bool    create_device        ();
            void    create_render_target ();
            void    destroy_render_target();
            void    fit_to_content       ();
            LRESULT get_hit_test         (const HWND&   in_window,
                                          const LPARAM& in_lparam) const;
            LRESULT process_message      (const HWND&   in_window,
                                          const UINT&   in_message,
                                          const WPARAM& in_wparam,
                                          const LPARAM& in_lparam);
            void    render_frame         ();
            void    resize               (const UINT&   in_width,
                                          const UINT&   in_height);

            /* The path of the system font, found through the fonts folder that Windows reports. Empty if the font is not found. */
            static std::string get_system_font_path();

            static LRESULT CALLBACK window_proc(HWND   in_window,
                                                UINT   in_message,
                                                WPARAM in_wparam,
                                                LPARAM in_lparam);

            int                     m_buttons_bottom;
            int                     m_buttons_left;
            int                     m_caption_height;
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

#endif /* UTILS_UI_IMGUI_PLAYER_WINDOW_BASE_H */
