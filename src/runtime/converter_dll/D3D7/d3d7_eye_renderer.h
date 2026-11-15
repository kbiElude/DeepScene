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
#include "stereo/core/stereo_core.h"
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
        /* Renders the eyes of a Direct3D 7 application whose vertices are in picture space (see ScreenParallax).
         *
         * The application draws the left eye with its own calls. The renderer draws the right eye, each time before the left eye. Every
         * surface the application renders to has a twin (see D3D7SurfaceTwins). When the application draws or clears, the renderer repeats
         * the call for the right eye: it renders to the twin, sets the twins of the textures and moves the vertices for the eye. When the
         * core says a draw is to be moved, the renderer also makes the left-eye draw itself, from a moved copy of the vertices, and the
         * application's own call is not made. The device is called through its virtual table, so nothing that watches the device sees the
         * calls that the renderer makes inside the interceptor callbacks.
         *
         * Some actions of the application cannot be followed: a draw from a vertex buffer that cannot be read, a copy or a load to a
         * surface whose twin cannot repeat it, a draw to a surface without a twin. They are counted and noted, and the right eye then
         * shows wrong content.
         *
         * Not thread-safe: the caller serializes the calls, and the device belongs to one thread.
         */
        class D3D7EyeRenderer
        {
        public:
            /* Returns the data chunk with the given id. The analysis runs before the renderer and takes the chunks out of the interceptor, so the renderer reads them through this function. */
            typedef std::function<bool(const uint32_t& in_chunk_id,
                                       const void**    out_data_ptr_ptr,
                                       uint32_t*       out_n_bytes_ptr)> ChunkFunc;

            /* The windowed mode gives the factor by which the back buffer is made larger than the application asks (see D3D7WindowedMode::get_render_scale()). */
            D3D7EyeRenderer(Stereo::Core::StereoCore* in_core_ptr,
                            const D3D7WindowedMode*   in_windowed_mode_ptr);

            uint64_t get_n_failed_twins() const;
            uint64_t get_n_twins       () const;

            /* Counts the calls made on the back buffer in the application's pixels. The back buffer is larger than the application knows,
             * so these calls land in the wrong place. A write of the whole back buffer is not counted: it is stretched (see
             * copy_pixel_write_to_both_eyes()).
             */
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

            /* A call that is not a draw is about to be made. The renderer tracks the state the call sets, and repeats a clear for the right eye.
             * Returns false if the application's call must not be made, because the renderer already made it with the sizes of the larger back buffer.
             */
            bool on_pre(const APIInterceptor::APIFunction&         in_function,
                        const uint32_t&                            in_n_arguments,
                        const APIInterceptor::APIFunctionArgument* in_arguments_ptr);

            /* A draw is about to be made, and the core has classified it (see StereoCore::classify_draw()). The renderer makes the draw for
             * the right eye. Returns false if the application's call must not be made, because the renderer made the left-eye draw itself.
             * in_picture_width is the width of the viewport that the draw uses (see CallFeeder::get_viewport()).
             */
            bool on_pre_draw(const APIInterceptor::APIFunction&         in_function,
                             const uint32_t&                            in_n_arguments,
                             const APIInterceptor::APIFunctionArgument* in_arguments_ptr,
                             const ChunkFunc&                           in_chunk_func,
                             const float&                               in_picture_width);

            /* Returns descriptions of the first calls that were not repeated for the right eye since the last call of this function. */
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

            /* The application locked the whole back buffer, wrote it and unlocked it. The picture is in the application's pixels, so it fills only
             * the corner of the larger back buffer. This function stretches it over the whole back buffer and copies it to the right eye. The
             * picture is flat, so it is the same in both eyes.
             */
            void copy_pixel_write_to_both_eyes(void* in_back_buffer_ptr) const;

            /* Makes the draw with the given vertices instead of the application's draw. */
            void draw(const DrawArguments& in_draw_arguments,
                      const uint8_t*       in_vertices_ptr) const;

            /* Makes the draw for one eye. If the core says the vertices are to be moved, the draw uses moved vertices. Otherwise it uses
             * the original vertices, but only if in_must_draw is set. Returns true if the draw was made.
             */
            bool draw_for_eye(const Analysis::Recipe::Eye& in_eye,
                              const DrawArguments&         in_draw_arguments,
                              const float&                 in_picture_width,
                              const bool&                  in_must_draw);

            /* Makes the device render to the twin of the surface the application set, with the twins of its textures. Returns false, and
             * changes nothing, if the surface has no twin.
             */
            bool enter_right_eye(void*          in_device_ptr,
                                 RightEyeScope* out_scope_ptr) const;

            /* How much larger the surface the application renders to is than the application knows: 1, except for the back buffer, which is made larger. */
            Stereo::Core::ResolutionPolicy get_render_policy() const;

            float get_render_scale() const;

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

            /* Repeats the application's clear for the right eye. The clear for the left eye is also made here if its rectangles must be
             * converted to the larger back buffer. Returns false if the application's call must not be made.
             */
            bool play_clear(const uint32_t&                            in_n_arguments,
                            const APIInterceptor::APIFunctionArgument* in_arguments_ptr);

            /* Sets the viewport that the application set in the device, converted to the pixels of the current render target. */
            void set_effective_viewport(void* in_device_ptr) const;

            /* Returns the bytes of a structure that an argument holds as a chunk; nullptr if the argument is not a chunk (a null pointer). */
            static const void* get_chunk_ptr(const APIInterceptor::APIFunctionArgument& in_argument);

            /* Reads the arguments of a draw call. Returns false if the call is not a draw that the renderer knows, or if its arguments are
             * not those of a draw.
             */
            static bool read_draw_arguments(const APIInterceptor::APIFunction&         in_function,
                                            const uint32_t&                            in_n_arguments,
                                            const APIInterceptor::APIFunctionArgument* in_arguments_ptr,
                                            const ChunkFunc&                           in_chunk_func,
                                            DrawArguments*                             out_draw_arguments_ptr);

            /* Reads the vertices of a draw from a vertex buffer into memory. The buffer is locked for reading, for the vertices that the draw
             * uses. The draw then uses vertices in memory, in the format of the buffer. Returns false if the buffer cannot be read.
             */
            bool read_vertex_buffer(DrawArguments* inout_draw_arguments_ptr);


            Stereo::Core::StereoCore* m_core_ptr;
            bool                      m_has_pixel_write; /* the whole back buffer is locked for writing
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
