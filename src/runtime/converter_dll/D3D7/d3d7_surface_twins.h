/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_CONVERTERDLL_D3D7_SURFACE_TWINS_H
#define STEREO_CONVERTERDLL_D3D7_SURFACE_TWINS_H

#include "common/api_function_argument.h"
#include "common/types.h"
#include "interception/hooks/D3D7/types.h"
#include <map>
#include <stdint.h>

namespace Stereo
{
    namespace ConverterDLL
    {
        /* Holds the right eye's copies of the surfaces that the application renders to. A surface that the application creates to render to
         * (a render target or a depth buffer), and the back buffer it gets from the primary surface, is created a second time with the same
         * size and format. That copy is its twin. The right eye is drawn to the twin while the left eye is drawn to the application's
         * surface. Attachments of a surface (for example the depth buffer of a render target) are attached between the twins in the same way.
         *
         * The surfaces are called through their virtual tables, so nothing that watches them sees the calls made inside the interceptor
         * callbacks. Not thread-safe: the caller serializes the calls.
         */
        class D3D7SurfaceTwins
        {
        public:
            D3D7SurfaceTwins();

            /* The twin of a surface of the application, or nullptr if it has none. */
            void* get_twin_ptr(void* in_surface_ptr) const;

            /* Whether the surface is the back buffer that the application got from its primary surface, that is, the surface it presents. */
            bool is_back_buffer(void* in_surface_ptr) const;

            uint64_t get_n_failed_twins() const;
            uint64_t get_n_twins       () const;

            /* The call has been made: creates the twins of the surfaces it created and releases the twins of the surfaces it destroyed. The
             * arguments are those of the call after it ran, including the surfaces it returned.
             */
            void on_post(const APIInterceptor::APIFunction&         in_function,
                         const uint32_t&                            in_n_arguments,
                         const APIInterceptor::APIFunctionArgument* in_arguments_ptr,
                         const APIInterceptor::APIFunctionArgument* in_returned_value_ptr);

        private:
            /* Reads the capabilities (DDSCAPS_*) from the surface itself; returns false if the surface cannot report them. */
            static bool get_caps(void*     in_surface_ptr,
                                 uint32_t* out_caps_ptr);

            /* Whether the call that returned the argument succeeded. A call without a returned value counts as succeeded. */
            static bool has_succeeded(const APIInterceptor::APIFunctionArgument* in_returned_value_ptr);

            void handle_add_attached_surface(const APIInterceptor::APIFunctionArgument* in_arguments_ptr);
            void handle_create_surface      (const APIInterceptor::APIFunctionArgument* in_arguments_ptr);
            void handle_get_attached_surface(const APIInterceptor::APIFunctionArgument* in_arguments_ptr);
            void handle_release             (const APIInterceptor::APIFunctionArgument* in_arguments_ptr,
                                             const APIInterceptor::APIFunctionArgument* in_returned_value_ptr);

            /* Creates the twin of the surface, with the same size and format, and stores it for the surface. On failure it counts the
             * failure and returns nullptr.
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
