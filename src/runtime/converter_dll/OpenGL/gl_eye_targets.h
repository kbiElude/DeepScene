/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_CONVERTERDLL_GL_EYE_TARGETS_H
#define STEREO_CONVERTERDLL_GL_EYE_TARGETS_H

#include "analysis/recipe/types.h"
#include "gl_extension_functions.h"
#include "stereo/core/types.h"
#include <array>
#include <functional>
#include <stdint.h>
#include <vector>

namespace Stereo
{
    namespace ConverterDLL
    {
        /* Render targets of an OpenGL application: one framebuffer per eye plus a small one holding a reduced copy of the left eye's depth.
         * The application renders to the left eye instead of its window. Every call here is the converter's own, with the
         * application's context current. */
        class GLEyeTargets
        {
        public:
            /* Receives the depths of a finished read (rows as read_depth() gives them). The pointer is valid only during the call. */
            typedef std::function<void(const float* in_depth_ptr, const size_t& in_n_depths)> DepthFunc;

            GLEyeTargets();

            /* Starts an asynchronous read of the left eye's depth (first scaled to the small copy). out_slot_ptr receives the buffer number
             * for finish_depth_read(). Returns false if no buffer is available. The left eye is bound afterward. */
            bool begin_depth_read(uint32_t* out_slot_ptr);

            /* Binds the framebuffer of the eye for drawing and reading. Returns false if there are no framebuffers. */
            bool bind_eye(const Analysis::Recipe::Eye& in_eye);

            /* Copies the left eye to the window's back buffer if framebuffer blit is available; the left eye is bound afterward. */
            void blit_left_to_window();

            /* Makes the framebuffers for the context and size unless they exist for that context, size and picture (others are deleted first).
             * The picture is the rectangle holding what the application renders; depth and picture reads use only it.
             * Returns false if they cannot be made or the context is not the first one. */
            bool ensure(const void*                    in_context_ptr,
                        const uint32_t&                in_width,
                        const uint32_t&                in_height,
                        const Stereo::Core::PixelRect& in_picture);

            /* Passes the depths of the read in_slot to in_depth_func (waits if unfinished), then frees the buffer.
             * Returns false if in_slot has no read in progress (targets were remade). */
            bool finish_depth_read(const uint32_t&  in_slot,
                                   const DepthFunc& in_depth_func);

            /* Framebuffer and buffer extension functions found for the context (not exported by the OpenGL library). */
            const GLExtensionFunctions& get_functions() const;

            /* How many times the targets were released; a read from an earlier generation is lost. */
            uint32_t get_generation() const;

            uint32_t get_height() const;

            /* Where the application's picture is in the framebuffers. */
            const Stereo::Core::PixelRect& get_picture() const;

            uint32_t get_width() const;

            /* Whether begin_depth_read() can work: the GPU buffers and the small depth copy exist. */
            bool has_depth_read_buffers() const;

            bool is_ready() const;

            /* Reads the left eye's depth (0 near, 1 far), rows bottom up: the small copy if it exists, else the whole buffer; the
             * width and height say which. Returns false if there are no framebuffers. */
            bool read_depth(std::vector<float>* out_depth_f32_vec_ptr,
                            uint32_t*           out_width_ptr,
                            uint32_t*           out_height_ptr);

            /* Reads the eye's image as 8-bit B, G, R, A, rows bottom up, to out_pixels_ptr (null: start of the bound pixel-pack buffer).
             * The left eye is bound afterward. */
            bool read_eye(const Analysis::Recipe::Eye& in_eye,
                          void*                        out_pixels_ptr);

            /* Reads the left eye's picture scaled to in_width x in_height (the application's window size when eyes render larger) as
             * 8-bit R, G, B, A, rows bottom up, point-sampled on the GPU into a target remade on size change.
             * Returns false if the blit is missing, the target cannot be made, or the application's pixel-pack state would make
             * the read differ from its own. The left eye is bound afterward. */
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

            /* Copies a rectangle of the left eye (colors or depth per in_mask) into the destination framebuffer, scaled, scissor off.
             * The destination is bound for drawing and the left eye for reading. */
            void blit_left_eye(const Stereo::Core::PixelRect& in_source,
                               const uint32_t&                in_destination_framebuffer,
                               const uint32_t&                in_destination_width,
                               const uint32_t&                in_destination_height,
                               const GLbitfield&              in_mask);

            /* The whole left eye as a rectangle. */
            Stereo::Core::PixelRect get_whole_eye() const;

            bool make_target(const uint32_t& in_width,
                             const uint32_t& in_height,
                             EyeTarget*      out_target_ptr);

            void release_target(EyeTarget* inout_target_ptr);

            void release_targets();

            /* Whether the application has default pixel-pack settings (no offset, row length, byte swap) and no pack buffer bound. */
            static bool are_pack_modes_default();

            static const GLbitfield COLOR_BUFFER_BIT;
            static const GLbitfield DEPTH_BUFFER_BIT;
            static const GLenum     FRAMEBUFFER;

            /* How many depth reads can be in progress at once (one per pass, plus the previous frame's pending reads). */
            static constexpr size_t N_DEPTH_READ_SLOTS = 16;

            static const GLenum PACK_ALIGNMENT;
            static const GLenum PIXEL_PACK_BUFFER;
            static const GLenum PIXEL_PACK_BUFFER_BINDING;

            const void*                              m_context_ptr;
            std::array<uint32_t, N_DEPTH_READ_SLOTS> m_depth_buffer_array;         /* GPU buffers the depth is read into; made on first use */
            EyeTarget                                m_depth_probe;
            GLExtensionFunctions                     m_functions;
            uint32_t                                 m_generation;                 /* times the targets were released; detects reads whose buffers are gone */
            bool                                     m_has_functions;
            bool                                     m_has_depth_probe;
            bool                                     m_has_readback_target;
            uint32_t                                 m_height;
            std::array<bool, N_DEPTH_READ_SLOTS>     m_is_depth_buffer_busy_array; /* a read into the buffer is in progress and not finished */
            bool                                     m_is_ready;
            EyeTarget                                m_left;
            Stereo::Core::PixelRect                  m_picture;                    /* where the application's picture is in the eyes */
            uint32_t                                 m_probe_height;
            uint32_t                                 m_probe_width;
            uint32_t                                 m_readback_height;
            EyeTarget                                m_readback_target;            /* window-size picture read_picture() reads from; made once per size */
            uint32_t                                 m_readback_width;
            EyeTarget                                m_right;
            uint32_t                                 m_width;
        };
    }
}

#endif /* STEREO_CONVERTERDLL_GL_EYE_TARGETS_H */
