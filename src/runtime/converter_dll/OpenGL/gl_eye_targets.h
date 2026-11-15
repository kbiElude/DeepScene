/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_CONVERTERDLL_GL_EYE_TARGETS_H
#define STEREO_CONVERTERDLL_GL_EYE_TARGETS_H

#include "analysis/recipe/types.h"
#include "gl_extension_functions.h"
#include <array>
#include <functional>
#include <stdint.h>
#include <vector>

namespace Stereo
{
    namespace ConverterDLL
    {
        /* The render targets of an OpenGL application: one framebuffer per eye, plus a small framebuffer for reading depth. Each eye framebuffer
         * has a color renderbuffer and a combined depth and stencil renderbuffer.
         *
         * The application renders to the framebuffer of the left eye instead of its window. The window is never drawn to, so it does not matter
         * whether it is visible. The small framebuffer holds a reduced copy of the depth of the left eye, because reading the full depth buffer
         * of a framebuffer as large as an eye of the headset is too slow to do often. Every call here is the converter's own and is made with
         * the context of the application current.
         */
        class GLEyeTargets
        {
        public:
            /* Receives the depths of a finished read (rows as read_depth() gives them). The pointer is valid only during the call. */
            typedef std::function<void(const float* in_depth_ptr, const size_t& in_n_depths)> DepthFunc;

            GLEyeTargets();

            /* Starts reading the depth of the left eye into a buffer of the graphics card, without waiting. The depth is first scaled down to the
             * small copy that read_depth() reads. out_slot_ptr receives the number of the buffer, which finish_depth_read() takes. Returns false
             * if no buffer is available: the small copy or the buffers do not exist, or all buffers are in use. The left eye is bound afterward.
             */
            bool begin_depth_read(uint32_t* out_slot_ptr);

            /* Binds the framebuffer of the eye for drawing and reading. Returns false if there are no framebuffers. */
            bool bind_eye(const Analysis::Recipe::Eye& in_eye);

            /* Copies the image of the left eye to the back buffer of the window if the framebuffer blit is available, then binds the left eye again. */
            void blit_left_to_window();

            /* Makes the framebuffers for the context at the given size, unless they already exist for that context and size. Framebuffers of
             * another size are deleted first. From then on the application renders to the left one. Returns false if they cannot be made, or if the
             * context is not the first one (a second context is not converted).
             */
            bool ensure(const void*     in_context_ptr,
                        const uint32_t& in_width,
                        const uint32_t& in_height);

            /* Passes the depths of the read in_slot to in_depth_func, waiting for the read if it is not finished, and then frees the buffer.
             * Returns false if in_slot has no read in progress (the targets were made again since the read began).
             */
            bool finish_depth_read(const uint32_t&  in_slot,
                                   const DepthFunc& in_depth_func);

            const void* get_context_ptr() const;

            /* The framebuffer and buffer functions found for the context. They are extension functions that the OpenGL library does not export. */
            const GLExtensionFunctions& get_functions() const;

            /* How many times the targets were released. A read that began in an earlier generation is lost. */
            uint32_t get_generation() const;

            uint32_t get_height() const;

            uint32_t get_width() const;

            /* Whether begin_depth_read() can work: the buffers of the graphics card exist, and so does the small depth copy that is read into them. */
            bool has_depth_read_buffers() const;

            bool is_ready() const;

            /* Reads the depth of the left eye (0 at the near plane, 1 at the far plane), rows from the bottom up. It reads the small copy if one
             * exists and the whole depth buffer otherwise; the width and height say which size was read. Returns false if there are no framebuffers.
             */
            bool read_depth(std::vector<float>* out_depth_f32_vec_ptr,
                            uint32_t*           out_width_ptr,
                            uint32_t*           out_height_ptr);

            /* Reads the image of the eye as 8-bit B, G, R, A, rows from the bottom up. The pixels go to out_pixels_ptr. If that is null, they go
             * to the start of the buffer that is bound for pixel packing. The left eye is bound afterward.
             */
            bool read_eye(const Analysis::Recipe::Eye& in_eye,
                          void*                        out_pixels_ptr);

            /* Reads the picture of the left eye scaled to in_width x in_height as 8-bit R, G, B, A, rows from the bottom up. The size is the
             * window size of the application when the eyes are rendered larger. The graphics card scales the picture down by taking, for each
             * pixel, the pixel of the larger picture under its center, into a target of that size. The target is made once and made again when
             * the size changes. Returns false if the blit is missing, if the target cannot be made, or if the pixel-packing state of the
             * application (or a bound pack buffer) would make this read differ from the application's own read. The left eye is bound afterward.
             */
            bool read_picture(const uint32_t&       in_width,
                              const uint32_t&       in_height,
                              std::vector<uint8_t>* out_rgba_u8_vec_ptr);

        private:
            struct EyeTarget
            {
                uint32_t color_renderbuffer = 0;
                uint32_t depth_renderbuffer = 0;
                uint32_t framebuffer        = 0;
            };

            /* Copies the left eye (the colors, or the depth, as in_mask says) into the destination framebuffer, scaled to its size. The scissor
             * test is turned off during the blit because it would clip it. The destination is bound for drawing and the left eye for reading.
             */
            void blit_left_eye(const uint32_t&   in_destination_framebuffer,
                               const uint32_t&   in_destination_width,
                               const uint32_t&   in_destination_height,
                               const GLbitfield& in_mask);

            bool make_target(const uint32_t& in_width,
                             const uint32_t& in_height,
                             EyeTarget*      out_target_ptr);

            void release_target(EyeTarget* inout_target_ptr);

            void release_targets();

            /* Whether the application has the default pixel-pack settings (no offset, no row length, no byte swap) and no buffer bound for packing. */
            static bool are_pack_modes_default();

            static const GLbitfield COLOR_BUFFER_BIT;
            static const GLbitfield DEPTH_BUFFER_BIT;
            static const GLenum     FRAMEBUFFER;

            /* How many depth reads can be in progress at once. A pass of a frame starts one, and the reads of the previous frame are still pending. */
            static constexpr size_t N_DEPTH_READ_SLOTS = 16;

            static const GLenum PACK_ALIGNMENT;
            static const GLenum PIXEL_PACK_BUFFER;
            static const GLenum PIXEL_PACK_BUFFER_BINDING;

            const void*                              m_context_ptr;
            std::array<uint32_t, N_DEPTH_READ_SLOTS> m_depth_buffer_array;         /* buffers of the graphics card that the depth is read into; made on first use */
            EyeTarget                                m_depth_probe;
            GLExtensionFunctions                     m_functions;
            uint32_t                                 m_generation;                 /* how many times the targets were released; tells apart reads whose buffers are gone */
            bool                                     m_has_functions;
            bool                                     m_has_depth_probe;
            bool                                     m_has_readback_target;
            uint32_t                                 m_height;
            std::array<bool, N_DEPTH_READ_SLOTS>     m_is_depth_buffer_busy_array; /* a read into the buffer is in progress and not finished */
            bool                                     m_is_ready;
            EyeTarget                                m_left;
            uint32_t                                 m_probe_height;
            uint32_t                                 m_probe_width;
            uint32_t                                 m_readback_height;
            EyeTarget                                m_readback_target;            /* the picture at window size that read_picture() reads from; made once per size, not for each read */
            uint32_t                                 m_readback_width;
            EyeTarget                                m_right;
            uint32_t                                 m_width;
        };
    }
}

#endif /* STEREO_CONVERTERDLL_GL_EYE_TARGETS_H */
