/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_CONVERTERDLL_D3D7_EYE_RENDERER_H
#define STEREO_CONVERTERDLL_D3D7_EYE_RENDERER_H

#include "analysis/recipe/types.h"
#include "common/api_function_argument.h"
#include "common/types.h"
#include "interception/hooks/D3D7/types.h"
#include "stereo/core/recipe_stereo_planner.h"
#include "d3d7_surface_twins.h"
#include "d3d7_windowed_mode.h"
#include "types.h"
#include <array>
#include <functional>
#include <stdint.h>
#include <string>
#include <vector>

namespace Stereo
{
    namespace ConverterDLL
    {
        /* Draws the right eye of a Direct3D 7 application with picture-space vertices; the application draws the left eye.
         * Repeats each draw/clear on the surface twins (see D3D7SurfaceTwins); calls go through the device's virtual table.
         * Actions that cannot be followed are counted and the right eye shows wrong content.
         * Not thread-safe: the caller serializes the calls; the device belongs to one thread. */
        class D3D7EyeRenderer
        {
        public:
            /* Returns the data chunk with the given id (the analysis takes the chunks out of the interceptor first). */
            typedef std::function<bool(const uint32_t& in_chunk_id,
                                       const void**    out_data_ptr_ptr,
                                       uint32_t*       out_n_bytes_ptr)> ChunkFunc;

            /* Back buffer enlargement factor (see D3D7WindowedMode::get_render_scale()). */
            D3D7EyeRenderer(Stereo::Core::RecipeStereoPlanner* in_core_ptr,
                            const D3D7WindowedMode*            in_windowed_mode_ptr);

            uint64_t get_n_failed_twins() const;
            uint64_t get_n_twins       () const;

            /* Counts calls on the back buffer in the application's pixels; they land in the wrong place because the buffer is larger.
             * Whole-buffer writes are not counted (stretched, see copy_pixel_write_to_both_eyes()). */
            uint64_t get_n_pixel_calls() const;

            /* Counts the calls that changed the surfaces of the right eye but were not repeated for it. */
            uint64_t get_n_unmirrored_calls() const;

            /* The twin of a surface of the application, or nullptr if it has none. */
            void* get_twin_ptr(void* in_surface_ptr) const;

            /* The call has been made: creates the twins of the surfaces it created and releases the twins of the surfaces it destroyed. */
            void on_post(const APIInterceptor::APIFunction&         in_function,
                         const uint32_t&                            in_n_arguments,
                         const APIInterceptor::APIFunctionArgument* in_arguments_ptr,
                         const APIInterceptor::APIFunctionArgument* in_returned_value_ptr);

            /* A non-draw call is about to be made: tracks its state, repeats clears for the right eye.
             * Returns false if the application's call must not be made (the renderer made it at the larger back buffer size). */
            bool on_pre(const APIInterceptor::APIFunction&         in_function,
                        const uint32_t&                            in_n_arguments,
                        const APIInterceptor::APIFunctionArgument* in_arguments_ptr);

            /* A draw is about to be made, classified by the core (see RecipeStereoPlanner::classify_draw()); draws the right eye.
             * Returns false if the application's call must not be made (the left-eye draw was made here).
             * in_picture_width: width of the draw's viewport (see LiveCallAnalyzer::get_viewport()). */
            bool on_pre_draw(const APIInterceptor::APIFunction&         in_function,
                             const uint32_t&                            in_n_arguments,
                             const APIInterceptor::APIFunctionArgument* in_arguments_ptr,
                             const ChunkFunc&                           in_chunk_func,
                             const float&                               in_picture_width);

            /* Returns descriptions of the first calls not repeated for the right eye since the last call. */
            std::vector<std::string> take_unmirrored_notes();


        private:
            static constexpr uint32_t N_TEXTURE_STAGES = 8;

            /* The arguments of a draw, in the order of the call. */
            struct DrawArguments
            {
                void*                                                  device_ptr        = nullptr;
                uint32_t                                               flags             = 0;
                uint32_t                                               fvf               = 0;
                const void*                                            indices_ptr       = nullptr;
                bool                                                   is_indexed        = false;
                DrawKind                                               kind              = DrawKind::VERTICES_IN_MEMORY;
                uint32_t                                               n_indices         = 0;
                uint32_t                                               n_vertices        = 0;
                uint32_t                                               primitive_type    = 0;
                uint32_t                                               start_vertex      = 0;
                Interception::Hooks::D3D7::D3DDRAWPRIMITIVESTRIDEDDATA strided           = {};
                void*                                                  vertex_buffer_ptr = nullptr;
                const void*                                            vertices_ptr      = nullptr;
            };

            /* The device state that the application has set, as far as the renderer has seen it. */
            struct DeviceMirror
            {
                bool                                    has_viewport      = false;
                void*                                   render_target_ptr = nullptr;
                std::array<void*, N_TEXTURE_STAGES>     texture_ptr_array = {};
                Interception::Hooks::D3D7::D3DVIEWPORT7 viewport          = {}; /* the application's, in its own pixels */
            };

            /* The device state that is changed to draw the right eye. It is restored afterwards. */
            struct RightEyeScope
            {
                std::array<bool, N_TEXTURE_STAGES>       is_texture_swapped = {};
                Interception::Hooks::D3D7::D3DVIEWPORT7  viewport           = {};
            };

            /* The application wrote the whole back buffer; the picture fills only a corner of the larger buffer.
             * Stretches it over the buffer and copies it to the right eye (flat, same in both eyes). */
            void copy_pixel_write_to_both_eyes(void* in_back_buffer_ptr) const;

            /* Makes the draw with the given vertices instead of the application's draw. */
            void draw(const DrawArguments& in_draw_arguments,
                      const uint8_t*       in_vertices_ptr) const;

            /* Makes the draw for one eye with moved vertices if the core says so, else with the original ones, only if in_must_draw.
             * Returns true if the draw was made. */
            bool draw_for_eye(const Analysis::Recipe::Eye& in_eye,
                              const DrawArguments&         in_draw_arguments,
                              const float&                 in_picture_width,
                              const bool&                  in_must_draw);

            /* Renders to the twin of the application's surface, with twin textures. Returns false, changing nothing, if there is no twin. */
            bool enter_right_eye(void*          in_device_ptr,
                                 RightEyeScope* out_scope_ptr) const;

            /* Where the application's pixels are in the surface it renders to (empty: all of it). */
            Stereo::Core::PixelRect get_render_picture() const;

            /* Size factor of the render surface over what the application knows: 1 except for the enlarged back buffer. */
            Stereo::Core::ResolutionPolicy get_render_policy() const;

            float get_render_scale() const;

            /* True if the application's pixels are scaled or moved in the surface (only the back buffer is). */
            bool is_render_layout_changed() const;

            /* Sets the state that the application set in the device again. */
            void leave_right_eye(void*                inout_device_ptr,
                                 const RightEyeScope& in_scope) const;

            /* Repeats a copy that the application made to a surface with a twin, onto the twin. Returns false if it could not be repeated. */
            bool mirror_blt(const APIInterceptor::APIFunction&         in_function,
                            const uint32_t&                            in_n_arguments,
                            const APIInterceptor::APIFunctionArgument* in_arguments_ptr) const;

            /* Counts a call that was not repeated for the right eye, and keeps a description of the first ones. */
            void note_unmirrored(const APIInterceptor::APIFunction& in_function,
                                 const char*                        in_reason_ptr);

            /* Repeats a clear for the right eye; also makes the left-eye clear if its rectangles need converting to the larger back buffer.
             * Returns false if the application's call must not be made. */
            bool play_clear(const uint32_t&                            in_n_arguments,
                            const APIInterceptor::APIFunctionArgument* in_arguments_ptr);

            /* Sets the viewport that the application set in the device, converted to the pixels of the current render target. */
            void set_effective_viewport(void* in_device_ptr) const;

            /* Returns the bytes of a structure that an argument holds as a chunk; nullptr if the argument is not a chunk (a null pointer). */
            static const void* get_chunk_ptr(const APIInterceptor::APIFunctionArgument& in_argument);

            /* Reads the arguments of a draw call. Returns false if it is not a known draw or its arguments do not fit. */
            static bool read_draw_arguments(const APIInterceptor::APIFunction&         in_function,
                                            const uint32_t&                            in_n_arguments,
                                            const APIInterceptor::APIFunctionArgument* in_arguments_ptr,
                                            const ChunkFunc&                           in_chunk_func,
                                            DrawArguments*                             out_draw_arguments_ptr);

            /* Reads the vertices a draw uses from a vertex buffer into memory (buffer format). Returns false if the buffer cannot be read. */
            bool read_vertex_buffer(DrawArguments* inout_draw_arguments_ptr);


            Stereo::Core::RecipeStereoPlanner* m_core_ptr;
            bool                               m_has_pixel_write; /* the whole back buffer is locked for writing
 */
            DeviceMirror              m_mirror;
            bool                      m_is_right_eye_off;
            uint64_t                  m_n_pixel_calls;
            uint64_t                  m_n_unmirrored_calls;
            D3D7SurfaceTwins          m_surface_twins;
            std::vector<uint8_t>      m_buffer_vertex_vec; /* the vertices read from a vertex buffer; the allocation is reused
 */
            std::vector<std::string>  m_unmirrored_note_vec;
            std::vector<uint8_t>      m_vertex_vec; /* the moved vertices of the draw being made; the allocation is reused
 */
            const D3D7WindowedMode*   m_windowed_mode_ptr;
        };
    }
}

#endif /* STEREO_CONVERTERDLL_D3D7_EYE_RENDERER_H */
