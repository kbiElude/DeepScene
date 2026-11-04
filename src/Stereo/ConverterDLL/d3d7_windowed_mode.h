/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_CONVERTERDLL_D3D7_WINDOWED_MODE_H
#define STEREO_CONVERTERDLL_D3D7_WINDOWED_MODE_H

#include "Common/types.h"
#include "invisible_windows.h"
#include <stdint.h>

namespace Stereo
{
    namespace ConverterDLL
    {
        /* Has a Direct3D 7 application render to a window, and makes the window invisible, so that the images of the application are seen
         * where the converter shows them and not on the screen of the application: an application which asks for the whole screen would
         * otherwise have it, in the display mode it asks for.
         *
         * The hooks of Direct3D 7 do not let a call be changed as it is made, so a call that has to be different is not made, and the
         * converter makes its own in its place, and writes what the application would have got where it is to get it (see the callbacks of
         * the interceptor): the application is not seen to be changed by what watches the calls. The cooperative level is the one of a
         * window, the display mode is not set (the window keeps the desktop's), and the primary surface, which a window cannot flip, is
         * made without the chain of buffers it was asked for; the back buffer that the application gets from it is a surface of its own,
         * of the size the display mode was asked for, which the application renders to, and a flip shows nothing. The window is the
         * application's own, made fully transparent and click-through, and kept so.
         *
         * Not thread-safe: the caller serializes the calls.
         */
        class D3D7WindowedMode
        {
        public:
            D3D7WindowedMode();

            /* The surface that the application renders to instead of the back buffer of the chain it asked for; nullptr until it has one. */
            void* get_back_buffer_ptr() const;

            /* Makes the windows invisible again if the application has changed their styles. */
            void keep_windows_invisible();

            /* A call is about to be made. Returns false if it is not to be made: what it was for is done, in the way of a window. */
            bool on_pre(const APIInterceptor::APIFunction&         in_function,
                        const uint32_t&                            in_n_arguments,
                        const APIInterceptor::APIFunctionArgument* in_arguments_ptr);

            /* Whether the application is made to render to a window, which is invisible: when the images are shown by another program (which says so
             * before the application starts), and not when the application is on the screen as it is.
             */
            void set_is_enabled(const bool& in_is_enabled);

        private:
            void handle_create_surface(const APIInterceptor::APIFunctionArgument* in_arguments_ptr,
                                       bool*                                      out_should_pass_through_ptr);

            void handle_get_attached_surface(const APIInterceptor::APIFunctionArgument* in_arguments_ptr,
                                             bool*                                      out_should_pass_through_ptr);

            void handle_set_cooperative_level(const APIInterceptor::APIFunctionArgument* in_arguments_ptr);

            void handle_set_display_mode(const APIInterceptor::APIFunctionArgument* in_arguments_ptr);

            /* Reads a number of an argument that is of 32 bits; false if it is not. */
            static bool read_u32(const APIInterceptor::APIFunctionArgument& in_argument,
                                 uint32_t*                                  out_value_ptr);


            void*            m_back_buffer_ptr;
            uint32_t         m_display_height;
            uint32_t         m_display_width;
            InvisibleWindows m_invisible_windows;
            bool             m_is_enabled;
            void*            m_primary_ptr;
        };
    }
}

#endif /* STEREO_CONVERTERDLL_D3D7_WINDOWED_MODE_H */
