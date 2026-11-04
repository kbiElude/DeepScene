/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_CONVERTERDLL_D3D7_EYE_RENDERER_H
#define STEREO_CONVERTERDLL_D3D7_EYE_RENDERER_H

#include "Common/types.h"
#include "Stereo/Core/stereo_core.h"
#include "d3d7_surface_twins.h"
#include <stdint.h>
#include <vector>

namespace Stereo
{
    namespace ConverterDLL
    {
        /* Renders the eyes of a Direct3D 7 application whose vertices are in the space of the picture (see ScreenParallax). The eye whose
         * images the application draws, by its own calls, is the left: a draw which the core says is to be moved is not made as the
         * application makes it, but by the renderer, with a copy of its vertices that is moved for the eye. The device is called through its
         * virtual table, which what is done inside the callbacks of the interceptor is not seen by anything that watches the device.
         *
         * For now this is the left eye alone: the right is not drawn yet.
         *
         * Not thread-safe: the caller serializes the calls, and the device is the one thread's.
         */
        class D3D7EyeRenderer
        {
        public:
            explicit D3D7EyeRenderer(Stereo::Core::StereoCore* in_core_ptr);

            uint64_t get_n_failed_twins() const;
            uint64_t get_n_twins       () const;

            /* The call is done: the twins of the surfaces it made are made, and those of the surfaces it ended are let go of. */
            void on_post(const APIInterceptor::APIFunction&         in_function,
                         const uint32_t&                            in_n_arguments,
                         const APIInterceptor::APIFunctionArgument* in_arguments_ptr,
                         const APIInterceptor::APIFunctionArgument* in_returned_value_ptr);

            /* A draw is about to be made, and the core has classified it (see StereoCore::classify_draw()). Returns false if the call of
             * the application is not to be made: the renderer made the draw itself. The picture width is that of the viewport the draw is
             * made with (see CallFeeder::get_viewport()).
             */
            bool on_pre_draw(const APIInterceptor::APIFunction&         in_function,
                             const uint32_t&                            in_n_arguments,
                             const APIInterceptor::APIFunctionArgument* in_arguments_ptr,
                             const float&                               in_picture_width);

        private:
            /* The arguments of a draw of vertices which the application holds in its own memory, in the order of the call. */
            struct DrawArguments
            {
                void*       device_ptr       = nullptr;
                uint32_t    flags            = 0;
                uint32_t    fvf              = 0;
                const void* indices_ptr      = nullptr;
                bool        is_indexed       = false;
                uint32_t    n_indices        = 0;
                uint32_t    n_vertices       = 0;
                uint32_t    primitive_type   = 0;
                const void* vertices_ptr     = nullptr;
            };

            /* Makes the draw with the vertices, in the place of the one the application made. */
            void draw(const DrawArguments& in_draw_arguments,
                      const uint8_t*       in_vertices_ptr) const;

            /* Reads the arguments of the call, which is of a draw this knows. Returns false if it is not one, or the arguments are not
             * what a draw has.
             */
            static bool read_draw_arguments(const APIInterceptor::APIFunction&         in_function,
                                            const uint32_t&                            in_n_arguments,
                                            const APIInterceptor::APIFunctionArgument* in_arguments_ptr,
                                            DrawArguments*                             out_draw_arguments_ptr);

            static bool read_u32(const APIInterceptor::APIFunctionArgument& in_argument,
                                 uint32_t*                                  out_value_ptr);


            Stereo::Core::StereoCore* m_core_ptr;
            D3D7SurfaceTwins          m_surface_twins;
            std::vector<uint8_t>      m_vertex_vec; /* of the draw being made: its room is used again */
        };
    }
}

#endif /* STEREO_CONVERTERDLL_D3D7_EYE_RENDERER_H */
