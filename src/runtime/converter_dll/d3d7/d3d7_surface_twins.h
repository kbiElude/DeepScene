/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_CONVERTERDLL_D3D7_SURFACE_TWINS_H
#define STEREO_CONVERTERDLL_D3D7_SURFACE_TWINS_H

#include "Common/types.h"
#include "Interception/Hooks/D3D7/types.h"
#include <map>
#include <stdint.h>

namespace Stereo
{
    namespace ConverterDLL
    {
        /* The right eye's copies of the surfaces that the application renders to. A surface that the application makes to render to (a
         * render target, a depth buffer), and the back buffer it gets from the primary surface, is made again, the same size and format, as
         * the application makes it: that is its twin, which the right eye is drawn to while the left eye is drawn to the surface. The
         * attachments of a surface (the depth buffer of a render target) are made again between the twins.
         *
         * The surfaces are called through their virtual tables, which what is done inside the callbacks of the interceptor is not seen by
         * anything that watches them. Not thread-safe: the caller serializes the calls.
         */
        class D3D7SurfaceTwins
        {
        public:
            D3D7SurfaceTwins();

            /* The twin of a surface of the application, or nullptr if it has none. */
            void* get_twin_ptr(void* in_surface_ptr) const;

            /* Whether the surface is the back buffer which the application got from its primary surface: what it presents. */
            bool is_back_buffer(void* in_surface_ptr) const;

            uint64_t get_n_failed_twins() const;
            uint64_t get_n_twins       () const;

            /* The call is done: makes the twins of what it made, and lets go of those of what it ended. The arguments are those of the call
             * after it, with the surfaces it gave.
             */
            void on_post(const APIInterceptor::APIFunction&         in_function,
                         const uint32_t&                            in_n_arguments,
                         const APIInterceptor::APIFunctionArgument* in_arguments_ptr,
                         const APIInterceptor::APIFunctionArgument* in_returned_value_ptr);

        private:
            /* The capabilities of the surface (DDSCAPS_*), which it says itself; false if it does not. */
            static bool get_caps(void*     in_surface_ptr,
                                 uint32_t* out_caps_ptr);

            void handle_add_attached_surface(const APIInterceptor::APIFunctionArgument* in_arguments_ptr);
            void handle_create_surface      (const APIInterceptor::APIFunctionArgument* in_arguments_ptr);
            void handle_get_attached_surface(const APIInterceptor::APIFunctionArgument* in_arguments_ptr);
            void handle_release             (const APIInterceptor::APIFunctionArgument* in_arguments_ptr,
                                             const APIInterceptor::APIFunctionArgument* in_returned_value_ptr);

            /* Makes the twin of the surface, the same size and format, and keeps it for the surface; nullptr, and a failure counted, if it
             * cannot be made.
             */
            void* make_twin_ptr(void*       in_surface_ptr,
                                const bool& in_is_back_buffer);


            void*                  m_back_buffer_ptr;
            void*                  m_direct_draw_ptr;
            uint64_t               m_n_failed_twins;
            void*                  m_primary_ptr;
            std::map<void*, void*> m_twin_map; /* the application's surface -> its twin */
        };
    }
}

#endif /* STEREO_CONVERTERDLL_D3D7_SURFACE_TWINS_H */
