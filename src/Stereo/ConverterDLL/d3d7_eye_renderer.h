/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_CONVERTERDLL_D3D7_EYE_RENDERER_H
#define STEREO_CONVERTERDLL_D3D7_EYE_RENDERER_H

#include "Analysis/RecipeLoader/types.h"
#include "Common/types.h"
#include "Interception/Hooks/D3D7/types.h"
#include "Stereo/Core/stereo_core.h"
#include "d3d7_surface_twins.h"
#include "d3d7_windowed_mode.h"
#include <array>
#include <stdint.h>
#include <string>
#include <vector>

namespace Stereo
{
    namespace ConverterDLL
    {
        /* Renders the eyes of a Direct3D 7 application whose vertices are in the space of the picture (see ScreenParallax). The eye whose
         * images the application draws, by its own calls, is the left; the right is drawn by the renderer, each time before the left: every
         * surface the application renders to has a twin (see D3D7SurfaceTwins), and a draw (or a clear) that the application makes is made
         * again with the twin of what it renders to, and of the textures it sets, in the device, with the vertices moved for the eye. A draw
         * that the core says is to be moved is not made as the application makes it for the left eye either, but by the renderer, with a copy
         * of its vertices that is moved. The device is called through its virtual table, which what is done inside the callbacks of the
         * interceptor is not seen by anything that watches the device.
         *
         * What the application does that the renderer cannot follow (it draws with vertex buffers, it copies to a surface which has a twin)
         * is counted, and what the right eye shows is then wrong.
         *
         * Not thread-safe: the caller serializes the calls, and the device is the one thread's.
         */
        class D3D7EyeRenderer
        {
        public:
            /* The windowed mode says how much larger than the application asks the back buffer is made (see D3D7WindowedMode::get_render_scale()). */
            D3D7EyeRenderer(Stereo::Core::StereoCore*  in_core_ptr,
                            const D3D7WindowedMode*    in_windowed_mode_ptr);

            uint64_t get_n_failed_twins() const;
            uint64_t get_n_twins       () const;

            /* How many calls were made on the back buffer in the pixels of the application, which are then not where they were meant to be. */
            uint64_t get_n_pixel_calls() const;

            /* How many calls were not made again for the right eye, though they changed what it holds. */
            uint64_t get_n_unmirrored_calls() const;

            /* The twin of a surface of the application, or nullptr if it has none. */
            void* get_twin_ptr(void* in_surface_ptr) const;

            /* The call is done: the twins of the surfaces it made are made, and those of the surfaces it ended are let go of. */
            void on_post(const APIInterceptor::APIFunction&         in_function,
                         const uint32_t&                            in_n_arguments,
                         const APIInterceptor::APIFunctionArgument* in_arguments_ptr,
                         const APIInterceptor::APIFunctionArgument* in_returned_value_ptr);

            /* A call that is not a draw is about to be made: what it sets is followed, and a clear is made for the right eye as well. Returns false
             * if the call is not to be made as the application makes it: the renderer made it, in the sizes of the larger back buffer.
             */
            bool on_pre(const APIInterceptor::APIFunction&         in_function,
                        const uint32_t&                            in_n_arguments,
                        const APIInterceptor::APIFunctionArgument* in_arguments_ptr);

            /* A draw is about to be made, and the core has classified it (see StereoCore::classify_draw()). The draw is made for the right eye;
             * returns false if the call of the application is not to be made, because the renderer made the draw for the left eye itself. The
             * picture width is that of the viewport the draw is made with (see CallFeeder::get_viewport()).
             */
            bool on_pre_draw(const APIInterceptor::APIFunction&         in_function,
                             const uint32_t&                            in_n_arguments,
                             const APIInterceptor::APIFunctionArgument* in_arguments_ptr,
                             const float&                               in_picture_width);

            /* The words that tell which calls were not made for the right eye, the first ones, since this was last called. */
            std::vector<std::string> take_unmirrored_notes();


        private:
            static constexpr uint32_t N_TEXTURE_STAGES = 8;

            /* The arguments of a draw of vertices which the application holds in its own memory, in the order of the call. */
            struct DrawArguments
            {
                void*       device_ptr     = nullptr;
                uint32_t    flags          = 0;
                uint32_t    fvf            = 0;
                const void* indices_ptr    = nullptr;
                bool        is_indexed     = false;
                uint32_t    n_indices      = 0;
                uint32_t    n_vertices     = 0;
                uint32_t    primitive_type = 0;
                const void* vertices_ptr   = nullptr;
            };

            /* What the application has set in the device, as far as the renderer knows. */
            struct DeviceMirror
            {
                bool                                    has_viewport      = false;
                void*                                   render_target_ptr = nullptr;
                std::array<void*, N_TEXTURE_STAGES>     texture_ptr_array = {};
                Interception::Hooks::D3D7::D3DVIEWPORT7 viewport          = {}; /* the application's, in its own pixels */
            };

            /* What is changed in the device to draw for the right eye, and has to be put back. */
            struct RightEyeScope
            {
                std::array<bool, N_TEXTURE_STAGES>       is_texture_swapped = {};
                Interception::Hooks::D3D7::D3DVIEWPORT7  viewport           = {};
            };

            /* Makes the draw with the vertices, in the place of the one the application made. */
            void draw(const DrawArguments& in_draw_arguments,
                      const uint8_t*       in_vertices_ptr) const;

            /* Makes the draw for the eye, with its vertices moved if the core says they are to be (or, if the draw must be made, as it is
             * otherwise). Returns true if the draw was made.
             */
            bool draw_for_eye(const Analysis::RecipeLoader::Eye& in_eye,
                              const DrawArguments&               in_draw_arguments,
                              const float&                       in_picture_width,
                              const bool&                        in_must_draw);

            /* The device renders to the twin of what the application set, with the twins of its textures; false if what it renders to has no
             * twin, and nothing was changed.
             */
            bool enter_right_eye(void*          in_device_ptr,
                                 RightEyeScope* out_scope_ptr) const;

            /* How much larger the surface that the application renders to is than it knows: 1 unless it is the back buffer, which is made larger. */
            float get_render_scale() const;

            /* What the application set is in the device again. */
            void leave_right_eye(void*                inout_device_ptr,
                                 const RightEyeScope& in_scope) const;

            /* The copy that the application made to a surface which has a twin, made to the twin as well. Returns false if it could not be. */
            bool mirror_blt(const APIInterceptor::APIFunction&         in_function,
                            const uint32_t&                            in_n_arguments,
                            const APIInterceptor::APIFunctionArgument* in_arguments_ptr) const;

            /* Counts a call that was not made for the right eye, and keeps what it was, for the first ones. */
            void note_unmirrored(const APIInterceptor::APIFunction& in_function,
                                 const char*                        in_reason_ptr);

            /* A clear which the application made, made for the right eye, and for the left if its rectangles are to be of the larger back buffer. Returns
             * false if the call of the application is not to be made.
             */
            bool play_clear(const uint32_t&                            in_n_arguments,
                            const APIInterceptor::APIFunctionArgument* in_arguments_ptr);

            /* Puts in the device the viewport that the application set, in the pixels of what it renders to now. */
            void set_effective_viewport(void* in_device_ptr) const;

            /* The bytes of a structure which an argument holds as a chunk; nullptr if it is not one (a pointer that was null). */
            static const void* get_chunk_ptr(const APIInterceptor::APIFunctionArgument& in_argument);

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
            DeviceMirror              m_mirror;
            bool                      m_is_right_eye_off;
            uint64_t                  m_n_pixel_calls;
            uint64_t                  m_n_unmirrored_calls;
            D3D7SurfaceTwins          m_surface_twins;
            std::vector<std::string>  m_unmirrored_note_vec;
            std::vector<uint8_t>      m_vertex_vec; /* of the draw being made: its room is used again */
            const D3D7WindowedMode*   m_windowed_mode_ptr;
        };
    }
}

#endif /* STEREO_CONVERTERDLL_D3D7_EYE_RENDERER_H */
