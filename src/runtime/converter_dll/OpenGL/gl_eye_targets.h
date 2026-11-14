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
        /* The two framebuffers (color and depth with stencil, of renderbuffers) that an application of OpenGL renders to instead of its window, one for
         * each eye, and a small one that the depth of the left eye is copied to for it to be read (the depth buffer of a framebuffer that is as large as the
         * headset's eye is too much to read often). The application is given the one of the left eye and never the window, which is not drawn to, so that it
         * does not matter if the window can be seen. All calls are the converter's own and are made with the context of the application current.
         */
        class GLEyeTargets
        {
        public:
            /* What is done with the depths that a read gave (the same rows as read_depth() gives), while they can be looked at. */
            typedef std::function<void(const float* in_depth_ptr, const size_t& in_n_depths)> DepthFunc;

            GLEyeTargets();

            /* Starts a read of the depth of the left eye, brought down to the small copy that read_depth() reads, to a buffer of the graphics card, and does not wait for it: the
             * number of the buffer is given, for finish_depth_read(). False if there is no buffer to read to (the small copy or the buffers are not there, or all of them are in
             * use). The left eye is current afterward.
             */
            bool begin_depth_read(uint32_t* out_slot_ptr);

            /* Makes the framebuffer of the eye the one that is drawn to and read from; false if there are none. */
            bool bind_eye(const Analysis::Recipe::Eye& in_eye);

            /* Copies the image of the left eye to the window (its back buffer), if the blit is there, and makes the framebuffer of the left eye current again. */
            void blit_left_to_window();

            /* Makes the framebuffers for the context, in the size, if there are none of that size for it: of another size the old ones are deleted, and
             * the application renders to the left one from now on. Returns false if they cannot be made.
             */
            bool ensure(const void*     in_context_ptr,
                        const uint32_t& in_width,
                        const uint32_t& in_height);

            /* Looks at the depths that the read of the number gave (it waits if they are not there yet), with the function, and gives the buffer back. False if the read is not
             * one that was begun and not finished (the buffers were made again since).
             */
            bool finish_depth_read(const uint32_t&  in_slot,
                                   const DepthFunc& in_depth_func);

            const void* get_context_ptr() const;

            /* The functions of the framebuffers and buffers, as found for the context; they are the ones that are not in the library. */
            const GLExtensionFunctions& get_functions() const;

            /* How many times the targets were released: a read that was begun in another generation is lost. */
            uint32_t get_generation() const;

            uint32_t get_height() const;

            uint32_t get_width() const;

            /* Whether begin_depth_read() can work: there are the buffers of the graphics card, and the small copy of the depth that is read to them. */
            bool has_depth_read_buffers() const;

            bool is_ready() const;

            /* Reads the depth of the left eye (0 at the near plane, 1 at the far one), rows from the bottom up, from a copy of the depth buffer that is small if it can be
             * made (with the size it says), and from the whole of it if not; false if there are no framebuffers.
             */
            bool read_depth(std::vector<float>* out_depth_f32_vec_ptr,
                            uint32_t*           out_width_ptr,
                            uint32_t*           out_height_ptr);

            /* Reads the image of the eye, rows from the bottom up, as B, G, R, A of 8 bits, to where the pointer says, or if it is null to the buffer that is bound for the
             * pixels to be packed to (at its start); the left eye is current afterward.
             */
            bool read_eye(const Analysis::Recipe::Eye& in_eye,
                          void*                        out_pixels_ptr);

            /* Reads the picture of the left eye brought down to the size (the size of the window of the application, when the eyes are made larger), as R, G, B and A of 8 bits,
             * rows from the bottom up: the graphics card brings it down, by taking for each pixel the one of the larger picture that is under its centre, to a target of that size
             * that is made once and kept (it is made again when the size changes), and it is read from that, once. False if the blit is missing, the target cannot be made, or the
             * application has set the way pixels are packed (or a buffer that they go to) so that what is read here would not be what its own read gives. The left eye is current afterward.
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

            /* Copies the left eye (the colors, or the depth) to the framebuffer, scaled to its size, with the scissor test off since it would cut the blit: the framebuffer is
             * bound for drawing, and the left eye for reading.
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

            /* Whether the application packs pixels as it does when it has set nothing (no offset, no row length, no swap of bytes) and has no buffer bound for them. */
            static bool are_pack_modes_default();

            static const GLbitfield COLOR_BUFFER_BIT;
            static const GLbitfield DEPTH_BUFFER_BIT;
            static const GLenum     FRAMEBUFFER;

            /* How many reads of the depth can be on their way at once: a pass of a frame is one, and the frame before is still on its way. */
            static constexpr size_t N_DEPTH_READ_SLOTS = 16;

            static const GLenum PACK_ALIGNMENT;
            static const GLenum PIXEL_PACK_BUFFER;
            static const GLenum PIXEL_PACK_BUFFER_BINDING;

            const void*                              m_context_ptr;
            std::array<uint32_t, N_DEPTH_READ_SLOTS> m_depth_buffer_array;         /* the buffers of the graphics card that the depth is read to; made when they are first used */
            EyeTarget                                m_depth_probe;
            GLExtensionFunctions                     m_functions;
            uint32_t                                 m_generation;                 /* how many times the targets were released, to tell reads that were on their way to buffers that are gone */
            bool                                     m_has_functions;
            bool                                     m_has_depth_probe;
            bool                                     m_has_readback_target;
            uint32_t                                 m_height;
            std::array<bool, N_DEPTH_READ_SLOTS>     m_is_depth_buffer_busy_array; /* a read is on its way to the buffer, and is not finished */
            bool                                     m_is_ready;
            EyeTarget                                m_left;
            uint32_t                                 m_probe_height;
            uint32_t                                 m_probe_width;
            uint32_t                                 m_readback_height;
            EyeTarget                                m_readback_target;            /* the picture of the size of the window, which is read from; made once for a size, not for each read */
            uint32_t                                 m_readback_width;
            EyeTarget                                m_right;
            uint32_t                                 m_width;
        };
    }
}

#endif /* STEREO_CONVERTERDLL_GL_EYE_TARGETS_H */
