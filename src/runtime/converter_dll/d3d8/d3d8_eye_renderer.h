/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_CONVERTERDLL_D3D8_EYE_RENDERER_H
#define STEREO_CONVERTERDLL_D3D8_EYE_RENDERER_H

#include "analysis/recipe/types.h"
#include "Common/types.h"
#include "Interception/Hooks/D3D8/types.h"
#include "stereo/core/stereo_core.h"
#include <array>
#include <map>
#include <stdint.h>
#include <vector>

namespace Stereo
{
    namespace ConverterDLL
    {
        /* Renders the second eye of a Direct3D 8 application. The eye whose images the application draws, by its own calls, is the left;
         * the right is drawn by the renderer, each time after the left: every render target, depth buffer and texture the application
         * renders to has a twin, made when the application makes it, and a draw (or a clear, or a copy) that the application makes is
         * made again with the twins in place of what the application has set. The device is called through its virtual table, which what
         * is done inside the callbacks of the interceptor is not seen by anything that watches the device.
         *
         * What the application does that the renderer cannot follow (it locks a render target, it resets the device, it renders to a
         * surface that was not seen made) ends the application, through the core, rather than show wrong images.
         *
         * Not thread-safe: the caller serializes the calls, and the device is the one thread's.
         */
        class D3D8EyeRenderer
        {
        public:
            explicit D3D8EyeRenderer(Stereo::Core::StereoCore* in_core_ptr);

            /* The twin of the back buffer of the device, or nullptr if it has none (yet). */
            void* get_back_buffer_twin_ptr(void* in_device_ptr);

            /* What the application renders to now, as the renderer knows; nullptr before the device is made. */
            void* get_render_target_ptr() const;

            /* The call is done: makes what the right eye needs of it (a twin of what the call made, the call again with the twins). */
            void on_post(const APIInterceptor::APIFunction&         in_function,
                         const uint32_t&                            in_n_arguments,
                         const APIInterceptor::APIFunctionArgument* in_arguments_ptr);

            /* The call is about to be made. Returns false if it is not to be: the draw is skipped. */
            bool on_pre(const APIInterceptor::APIFunction&         in_function,
                        const uint32_t&                            in_n_arguments,
                        const APIInterceptor::APIFunctionArgument* in_arguments_ptr);

            /* A draw of the class is about to be made: puts in the device the matrices for the left eye. Returns false if it is not to be. */
            bool on_pre_draw(const Analysis::Recipe::DrawClass&         in_draw_class,
                             const APIInterceptor::APIFunctionArgument* in_arguments_ptr);

        private:
            static constexpr uint32_t N_TEXTURE_STAGES = 8;

            /* What the application has set in the device, as far as the renderer knows. */
            struct DeviceMirror
            {
                void*                                    back_buffer_ptr   = nullptr; /* what the device renders to to begin with: what is presented */
                void*                                    depth_stencil_ptr = nullptr;
                void*                                    render_target_ptr = nullptr;
                std::array<void*, N_TEXTURE_STAGES>      texture_ptr_array = {};
            };

            void  apply_matrices          (void*                                      in_device_ptr,
                                           const uint32_t&                            in_n_changes) const;
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
            void  repeat_call             (const APIInterceptor::APIFunction&         in_function,
                                           const APIInterceptor::APIFunctionArgument* in_arguments_ptr) const;

            /* The twin of a surface or a texture; nullptr if it has none. */
            void* get_twin_ptr(void* in_object_ptr) const;


            std::vector<Stereo::Core::MatrixChange> m_change_vec; /* of the draw being made: its entries are used again */
            Stereo::Core::StereoCore*               m_core_ptr;
            void*                                   m_device_ptr;
            Analysis::Recipe::DrawClass             m_draw_class;
            DeviceMirror                            m_mirror;
            std::map<void*, void*>                  m_twin_map;   /* the application's -> its twin */
        };
    }
}

#endif /* STEREO_CONVERTERDLL_D3D8_EYE_RENDERER_H */
