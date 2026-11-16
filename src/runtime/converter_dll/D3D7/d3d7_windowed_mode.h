/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_CONVERTERDLL_D3D7_WINDOWED_MODE_H
#define STEREO_CONVERTERDLL_D3D7_WINDOWED_MODE_H

#include "common/api_function_argument.h"
#include "common/types.h"
#include "interception/hooks/D3D7/types.h"
#include "stereo/core/resolution_policy.h"
#include "invisible_window_manager.h"
#include <stdint.h>

namespace Stereo
{
    namespace ConverterDLL
    {
        /* Makes a Direct3D 7 application render to a window and makes that window invisible. The converter then shows the application's
         * images; the application does not show them on the screen. Without this, an application that asks for the whole screen would take
         * it over, in the display mode it requests.
         *
         * The Direct3D 7 hooks cannot change a call while it is made. Where a call must differ, the converter does not make it. It makes
         * its own call instead and writes the results that the application would have received into the application's output arguments
         * (see the callbacks of the interceptor), so anything that watches the calls sees an unchanged application. The converter changes
         * the following:
         * - The cooperative level is the one for a window, and the display mode is not set, so the window keeps the desktop's mode.
         * - A window cannot flip, so the primary surface is created without the chain of buffers the application asked for.
         * - The back buffer that the application gets is a separate surface with the size the display mode was asked for. The application
         *   renders to it, and a flip shows nothing.
         * - The window is the application's own window. It is made fully transparent and click-through, and kept that way.
         *
         * Not thread-safe: the caller serializes the calls.
         */
        class D3D7WindowedMode
        {
        public:
            D3D7WindowedMode();

            /* The surface that the application renders to instead of the back buffer of the chain it asked for; nullptr until it is created. */
            void* get_back_buffer_ptr() const;

            /* The size of the display mode that the application asked for. It is also the size of the application's pictures. 0 until the application asks. */
            void get_display_size(uint32_t* out_width_ptr,
                                  uint32_t* out_height_ptr) const;

            /* The size of the back buffer (see RenderScalePlanner::choose_layout). 0 until the display mode is known. */
            uint32_t get_frame_height() const;
            uint32_t get_frame_width () const;

            /* Where the application's picture is in the back buffer. */
            const Stereo::Core::PixelRect& get_picture() const;

            /* How the back buffer is enlarged relative to what the application asks, to fit the images that are shown. It is the identity
             * until the size of the display mode is known, and when nothing is enlarged.
             */
            const Stereo::Core::ResolutionPolicy& get_policy() const;

            /* The factor by which the back buffer is larger than the application asks, to fit the images that are shown. It is 1 until the
             * size of the display mode is known, and when nothing is enlarged.
             */
            float get_render_scale() const;

            /* Returns the size in pixels that the back buffer gets for an extent that the application asks for. */
            uint32_t get_scaled_extent(const uint32_t& in_extent) const;

            /* True if the application's pixels are scaled or moved in the back buffer. */
            bool is_layout_changed() const;

            /* Makes the windows invisible again if the application changed their styles. */
            void keep_windows_invisible();

            /* A call is about to be made. Returns false if the call must not be made: the converter has done what the call was for, in the way a window needs it. */
            bool on_pre(const APIInterceptor::APIFunction&         in_function,
                        const uint32_t&                            in_n_arguments,
                        const APIInterceptor::APIFunctionArgument* in_arguments_ptr);

            /* Whether the application is made to render to an invisible window. This is the case when another program shows the images (it says
             * so before the application starts), and not when the application is shown on the screen as it is.
             */
            void set_is_enabled(const bool& in_is_enabled);

            /* The size of one eye's image in the program that shows the images (see FrameChannel::get_target_eye_size()). The back buffer is
             * made as large as this allows, keeping the proportions of the display mode.
             */
            void set_target_eye_size(const uint32_t& in_width,
                                     const uint32_t& in_height);


        private:
            void handle_create_surface(const APIInterceptor::APIFunctionArgument* in_arguments_ptr,
                                       bool*                                      out_should_pass_through_ptr);

            void handle_get_attached_surface(const APIInterceptor::APIFunctionArgument* in_arguments_ptr,
                                             bool*                                      out_should_pass_through_ptr);

            void handle_set_cooperative_level(const APIInterceptor::APIFunctionArgument* in_arguments_ptr);

            void handle_set_display_mode(const APIInterceptor::APIFunctionArgument* in_arguments_ptr);

            /* A render-target surface (for example a depth buffer) that is as large as the application's images is created at the size of the back buffer. */
            void handle_sized_surface(const Interception::Hooks::D3D7::DDSURFACEDESC2& in_description,
                                      void*                                            in_direct_draw_ptr,
                                      void**                                           out_surface_ptr_ptr,
                                      bool*                                            out_should_pass_through_ptr);

            void*                          m_back_buffer_ptr;
            uint32_t                       m_display_height;
            uint32_t                       m_display_width;
            uint32_t                       m_frame_height;
            uint32_t                       m_frame_width;
            InvisibleWindowManager         m_invisible_window_manager;
            bool                           m_is_enabled;
            Stereo::Core::PixelRect        m_picture;
            Stereo::Core::ResolutionPolicy m_policy;
            void*                          m_primary_ptr;
            uint32_t                       m_target_height;
            uint32_t                       m_target_width;
        };
    }
}

#endif /* STEREO_CONVERTERDLL_D3D7_WINDOWED_MODE_H */
