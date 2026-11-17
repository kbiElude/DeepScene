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
        /* Draws the right eye of a Direct3D 8 application after each call that draws the left eye, repeating draws, clears and copies
         * on twins of the render targets, depth buffers and textures; calls go through the device's virtual table.
         * XYZRHW (pixel) draws are made here for both eyes. It ends the application on what it cannot follow (locking a render target,
         * device reset, unseen surfaces, XYZRHW vertices from a vertex buffer).
         * Not thread-safe: the caller serializes the calls; the device belongs to one thread. */
        class D3D8EyeRenderer
        {
        public:
            explicit D3D8EyeRenderer(Stereo::Core::RecipeStereoPlanner* in_core_ptr);

            /* The twin of the back buffer of the device, or nullptr if there is none. */
            void* get_back_buffer_twin_ptr(void* in_device_ptr);

            /* Twins created, and twins wanted but not created (the device refused). */
            uint64_t get_n_failed_twins() const;
            uint64_t get_n_twins       () const;

            /* The tracked current render target; nullptr before the device is created. */
            void* get_render_target_ptr() const;

            /* The call is done: creates what the right eye needs (a twin, or the call repeated with twins). */
            void on_post(const APIInterceptor::APIFunction&         in_function,
                         const uint32_t&                            in_n_arguments,
                         const APIInterceptor::APIFunctionArgument* in_arguments_ptr);

            /* The call is about to be made. Returns false if the call must be skipped. */
            bool on_pre(const APIInterceptor::APIFunction&         in_function,
                        const uint32_t&                            in_n_arguments,
                        const APIInterceptor::APIFunctionArgument* in_arguments_ptr);

            /* A draw is about to be made: sets the left eye's matrices. Returns false if the application's call must not be made
             * (skipped, or already made for both eyes with pixel vertices). The picture width is the application viewport's width. */
            bool on_pre_draw(const Analysis::Recipe::DrawClass&         in_draw_class,
                             const APIInterceptor::APIFunction&         in_function,
                             const float&                               in_picture_width,
                             const APIInterceptor::APIFunctionArgument* in_arguments_ptr);

        private:
            static constexpr uint32_t N_TEXTURE_STAGES = 8;

            /* The device state that the application has set, as far as the renderer has seen it. */
            struct DeviceMirror
            {
                void*                                    back_buffer_ptr   = nullptr; /* the back buffer the device was created with (the one presented) */
                void*                                    depth_stencil_ptr = nullptr;
                void*                                    render_target_ptr = nullptr;
                std::array<void*, N_TEXTURE_STAGES>      texture_ptr_array = {};
                uint32_t                                 vertex_shader     = 0;       /* last SetVertexShader argument: shader handle (top bit set) or vertex format */
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

            /* The twin of a surface or texture, or the object itself if it has none; the right eye reads from this. */
            void* get_twin_or_own_ptr(void* in_object_ptr) const;

            /* The twin of a surface or a texture; nullptr if it has none. */
            void* get_twin_ptr(void* in_object_ptr) const;

            /* Returns the pointer that an argument holds, to pass to the device unchanged. */
            static void* get_pointer_argument(const APIInterceptor::APIFunctionArgument& in_argument);

            std::vector<Stereo::Core::MatrixChange> m_change_vec; /* of the current draw; entries are reused */
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
