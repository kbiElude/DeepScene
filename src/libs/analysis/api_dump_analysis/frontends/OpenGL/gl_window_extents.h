/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * The size of the windows of the program and of their device contexts, as the calls that make them say. A program need not call glViewport:
 * the viewport and the scissor box of a context start as the client area of the window that the context is first made current to, and
 * this is where that size comes from.
 */
#ifndef APIDUMPANALYSIS_FRONTENDS_OPENGL_GL_WINDOW_EXTENTS_H
#define APIDUMPANALYSIS_FRONTENDS_OPENGL_GL_WINDOW_EXTENTS_H

#include "capture/api_dump_loader/api_dump_loader.h"
#include <map>
#include <stdint.h>

namespace Analysis
{
    namespace APIDumpAnalysis
    {
        namespace Frontends
        {
            class GLWindowExtents
            {
            public:
                /* True for the calls that say a size: CreateWindowEx, and GetDC, which tells the window that a device context belongs to. */
                static bool follows_function(const APIInterceptor::APIFunction& in_function);

                /* The size of the client area of the window that the device context belongs to. Returns false if it is not known: the window was
                 * made with the default size, or by a call that was not followed.
                 */
                bool get_client_size(const uint64_t& in_device_context_address,
                                     uint32_t*       out_width_ptr,
                                     uint32_t*       out_height_ptr) const;

                /* Takes a call for which follows_function() is true. */
                void on_call(const Capture::APIDumpLoader::DumpedAPICall& in_call);

            private:
                struct ClientSize
                {
                    uint32_t height = 0;
                    uint32_t width  = 0;
                };

                /* The client area of a window made with the size of its outer rectangle and these styles. Returns false if it cannot be told. */
                static bool find_client_size(const uint32_t& in_style,
                                             const uint32_t& in_ex_style,
                                             const int32_t&  in_width,
                                             const int32_t&  in_height,
                                             const bool&     in_has_menu,
                                             ClientSize*     out_client_size_ptr);

                void on_create_window(const Capture::APIDumpLoader::DumpedAPICall& in_call);
                void on_get_dc       (const Capture::APIDumpLoader::DumpedAPICall& in_call);

                std::map<uint64_t, ClientSize> m_client_size_by_window_map; /* by the address of the window            */
                std::map<uint64_t, uint64_t>   m_window_by_dc_map;          /* the window of a device context, by address */
            };
        }
    }
}

#endif /* APIDUMPANALYSIS_FRONTENDS_OPENGL_GL_WINDOW_EXTENTS_H */
