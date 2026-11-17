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
        /* Makes a Direct3D 7 application render to an invisible window so the converter shows the images.
         * Hooks cannot change a call: the converter makes its own call and writes the expected results to the output arguments.
         * Cooperative level is windowed, no display mode is set, no buffer chain on the primary, the back buffer is a separate surface.
         * Not thread-safe: the caller serializes the calls. */
        class D3D7WindowedMode
        {
        public:
            D3D7WindowedMode();

            /* The surface the application renders to instead of the requested chain's back buffer; nullptr until created. */
            void* get_back_buffer_ptr() const;

            /* The requested display mode size, also the size of the application's pictures. 0 until requested. */
            void get_display_size(uint32_t* out_width_ptr,
                                  uint32_t* out_height_ptr) const;

            /* Where the application's picture is in the back buffer. */
            const Stereo::Core::PixelRect& get_picture() const;

            /* How the back buffer is enlarged relative to the request; identity until the display mode size is known or if not enlarged. */
            const Stereo::Core::ResolutionPolicy& get_policy() const;

            /* Factor by which the back buffer exceeds the request; 1 until the display mode size is known or if not enlarged. */
            float get_render_scale() const;

            /* True if the application's pixels are scaled or moved in the back buffer. */
            bool is_layout_changed() const;

            /* Makes the windows invisible again if the application changed their styles. */
            void keep_windows_invisible();

            /* A call is about to be made. Returns false if it must not be made (the converter already did its job in a window-compatible way). */
            bool on_pre(const APIInterceptor::APIFunction&         in_function,
                        const uint32_t&                            in_n_arguments,
                        const APIInterceptor::APIFunctionArgument* in_arguments_ptr);

            /* Whether the application renders to an invisible window (another program shows the images). Set before the application starts. */
            void set_is_enabled(const bool& in_is_enabled);

            /* Size of one eye's image in the showing program (see FrameChannel::get_target_eye_size()); caps the back buffer, keeping the display mode's aspect. */
            void set_target_eye_size(const uint32_t& in_width,
                                     const uint32_t& in_height);


        private:
            void handle_create_surface(const APIInterceptor::APIFunctionArgument* in_arguments_ptr,
                                       bool*                                      out_should_pass_through_ptr);

            void handle_get_attached_surface(const APIInterceptor::APIFunctionArgument* in_arguments_ptr,
                                             bool*                                      out_should_pass_through_ptr);

            void handle_set_cooperative_level(const APIInterceptor::APIFunctionArgument* in_arguments_ptr);

            void handle_set_display_mode(const APIInterceptor::APIFunctionArgument* in_arguments_ptr);

            /* A render-target surface (e.g. depth buffer) as large as the application's images gets the back buffer size. */
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
