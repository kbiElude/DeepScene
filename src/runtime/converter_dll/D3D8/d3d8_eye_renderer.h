/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_CONVERTERDLL_D3D8_EYE_RENDERER_H
#define STEREO_CONVERTERDLL_D3D8_EYE_RENDERER_H

#include "analysis/recipe/types.h"
#include "common/api_function_argument.h"
#include "common/types.h"
#include "interception/hooks/D3D8/types.h"
#include "stereo/core/recipe_stereo_planner.h"
#include <array>
#include <functional>
#include <map>
#include <stdint.h>
#include <vector>

namespace Stereo
{
    namespace ConverterDLL
    {
        /* Renders the right eye of a Direct3D 8 application. The application's own calls draw the left eye. The renderer draws the right
         * eye after each of those calls. Every render target, depth buffer and texture that the application renders to has a twin, which the
         * renderer creates when the application creates the original. A draw, clear or copy of the application is repeated with the twins in
         * place of the application's objects. The renderer calls the device through its virtual table, so anything that watches the device
         * does not see these calls.
         *
         * A draw with vertices in pixels (D3DFVF_XYZRHW) is made by the renderer for both eyes. The core's plan moves the vertices for each eye.
         *
         * When the renderer meets something it cannot follow, it ends the application instead of showing wrong images. This covers locking a
         * render target, resetting the device, rendering to a surface the renderer did not see created, and XYZRHW vertices from a vertex buffer.
         *
         * Not thread-safe. The caller serializes the calls, and the device belongs to one thread.
         */
        class D3D8EyeRenderer
        {
        public:
            explicit D3D8EyeRenderer(Stereo::Core::RecipeStereoPlanner* in_core_ptr);

            /* The twin of the back buffer of the device, or nullptr if there is none. */
            void* get_back_buffer_twin_ptr(void* in_device_ptr);

            /* The number of twins created, and the number of twins that were wanted but could not be created (the device refused to create a render
             * target or depth buffer again). */
            uint64_t get_n_failed_twins() const;
            uint64_t get_n_twins       () const;

            /* The render target that the application currently renders to, as the renderer tracked it. nullptr before the device is created. */
            void* get_render_target_ptr() const;

            /* The call is done: creates what the right eye needs for it (a twin of the object the call created, or the call repeated with the twins). */
            void on_post(const APIInterceptor::APIFunction&         in_function,
                         const uint32_t&                            in_n_arguments,
                         const APIInterceptor::APIFunctionArgument* in_arguments_ptr);

            /* The call is about to be made. Returns false if the call must be skipped. */
            bool on_pre(const APIInterceptor::APIFunction&         in_function,
                        const uint32_t&                            in_n_arguments,
                        const APIInterceptor::APIFunctionArgument* in_arguments_ptr);

            /* A draw is about to be made: sets the left eye's matrices in the device. Returns false if the application's call must not be made: either
             * the draw is skipped, or the renderer already made it for both eyes (vertices in pixels). The picture width is the width of the
             * application's viewport.
             */
            bool on_pre_draw(const Analysis::Recipe::DrawClass&         in_draw_class,
                             const APIInterceptor::APIFunction&         in_function,
                             const float&                               in_picture_width,
                             const APIInterceptor::APIFunctionArgument* in_arguments_ptr);

        private:
            static constexpr uint32_t N_TEXTURE_STAGES = 8;

            /* The device state that the application has set, as far as the renderer has seen it. */
            struct DeviceMirror
            {
                void*                                    back_buffer_ptr   = nullptr; /* the back buffer that the device was created with, which is what is presented */
                void*                                    depth_stencil_ptr = nullptr;
                void*                                    render_target_ptr = nullptr;
                std::array<void*, N_TEXTURE_STAGES>      texture_ptr_array = {};
                uint32_t                                 vertex_shader     = 0;       /* the last argument of SetVertexShader: a shader handle, or a vertex format (a handle has the top bit set) */
            };

            void  apply_matrices          (void*                                      in_device_ptr,
                                           const uint32_t&                            in_n_changes) const;
            void  draw_pre_transformed    (const APIInterceptor::APIFunction&         in_function,
                                           const APIInterceptor::APIFunctionArgument* in_arguments_ptr,
                                           const float&                               in_picture_width);
            void  draw_with_vertices      (const APIInterceptor::APIFunction&         in_function,
                                           const APIInterceptor::APIFunctionArgument* in_arguments_ptr,
                                           const uint8_t*                             in_vertices_ptr) const;
            void  handle_copy_rects       (const APIInterceptor::APIFunctionArgument* in_arguments_ptr);
            void  handle_create_device    (const APIInterceptor::APIFunctionArgument* in_arguments_ptr);
            void  handle_create_texture   (const APIInterceptor::APIFunctionArgument* in_arguments_ptr);
            void  handle_get_surface      (const APIInterceptor::APIFunctionArgument& in_surface_argument);
            void  handle_get_surface_level(const APIInterceptor::APIFunctionArgument* in_arguments_ptr);
            void  handle_update_texture   (const APIInterceptor::APIFunctionArgument* in_arguments_ptr);
            void* make_surface_twin_ptr   (void*                                      in_device_ptr,
                                           void*                                      in_surface_ptr);
            void  play_right_eye          (const APIInterceptor::APIFunction&         in_function,
                                           const APIInterceptor::APIFunctionArgument* in_arguments_ptr);
            void  play_right_eye          (void*                                      in_device_ptr,
                                           const std::function<void()>&               in_draw_func);
            void  repeat_call             (const APIInterceptor::APIFunction&         in_function,
                                           const APIInterceptor::APIFunctionArgument* in_arguments_ptr) const;

            /* The twin of a surface or a texture, or the object itself if it has no twin. The right eye reads from this. */
            void* get_twin_or_own_ptr(void* in_object_ptr) const;

            /* The twin of a surface or a texture; nullptr if it has none. */
            void* get_twin_ptr(void* in_object_ptr) const;

            /* Returns the pointer that an argument holds, to pass to the device unchanged. */
            static void* get_pointer_argument(const APIInterceptor::APIFunctionArgument& in_argument);

            std::vector<Stereo::Core::MatrixChange> m_change_vec; /* of the draw being made; the entries are reused */
            Stereo::Core::RecipeStereoPlanner*      m_core_ptr;
            void*                                   m_device_ptr;
            Analysis::Recipe::DrawClass             m_draw_class;
            bool                                    m_is_draw_handled; /* set when on_pre_draw() has made the draw for both eyes */
            DeviceMirror                            m_mirror;
            uint64_t                                m_n_failed_twins;
            std::map<void*, void*>                  m_twin_map;   /* the application's object -> its twin */
            std::vector<uint8_t>                    m_vertex_vec; /* the vertices of the draw, moved for one eye */
        };
    }
}

#endif /* STEREO_CONVERTERDLL_D3D8_EYE_RENDERER_H */
