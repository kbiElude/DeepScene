/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_CONVERTERDLL_OPENGL_TYPES_H
#define STEREO_CONVERTERDLL_OPENGL_TYPES_H

#include "common/types.h"
#include "stereo/core/types.h"
#include <array>
#include <memory>
#include <stdint.h>

namespace Stereo
{
    namespace ConverterDLL
    {
        /* What a call that is recorded sets for the vertices of a block (see GLBlockRecorder). */
        enum class RecordedCallKind
        {
            COLOR3,
            COLOR4,
            NORMAL3,
            TEXCOORD2,
            VERTEX2,
            VERTEX3
        };

        /* The channels of the color buffer of the window of an OpenGL application, as the pixel format of its device context reports them (see GLColorTargetFormats). */
        struct GLColorLayout
        {
            uint32_t alpha_bits = 0;
            uint32_t blue_bits  = 0;
            uint32_t green_bits = 0;
            bool     is_rgba    = false; /* false for a color-index pixel format */
            uint32_t red_bits   = 0;
        };

        /* The eye images of one frame of an OpenGL application, as OpenGL reads them: rows from the bottom up, 8 bits each of B, G, R and A (the
         * order of the channel's images). The pixels are in memory that is mapped from the buffers they were read into. The memory stays valid
         * while anything holds the token. The code that passes the images on keeps a copy of this struct until it is done with them.
         */
        struct GLFrameImages
        {
            std::shared_ptr<const void> consumer_token;
            uint32_t                    height          = 0;
            const uint8_t*              left_bgra_ptr   = nullptr;
            Stereo::Core::PixelRect     picture;        /* the picture in each eye image */
            const uint8_t*              right_bgra_ptr  = nullptr;
            uint32_t                    width           = 0;
        };

        /* A call between glBegin and glEnd, kept to be made again for each eye. */
        struct RecordedCall
        {
            RecordedCallKind     kind           = RecordedCallKind::COLOR3;
            std::array<float, 4> value_fp32vec4 = {};
        };

        /* What a function that can be recorded sets: the kind, how many numbers, and whether it gives them in memory (the functions that end in v). */
        struct RecordedCallLayout
        {
            bool             is_vector = false;
            RecordedCallKind kind      = RecordedCallKind::COLOR3;
            uint32_t         n_values  = 0;
        };

        /* A glReadPixels call of the application that reads one pixel of the colors, in bytes, into the application's own memory. It is the only read that the converter makes itself (see GLResolutionScaler). */
        struct SinglePixelRead
        {
            uint32_t format     = 0;
            void*    pixels_ptr = nullptr;
            uint32_t type       = 0;
            int64_t  x          = 0;
            int64_t  y          = 0;
        };

        /* What the textures that are bound for a draw need for each eye (see GLEyeRenderer::find_texture_selection()). */
        struct TextureSelection
        {
            bool     has_twin                 = false; /* the texture of the active unit has a twin for the right eye */
            bool     has_twin_in_another_unit = false; /* another unit has a texture that has one, which cannot be bound without changing the active unit */
            uint32_t texture                  = 0;     /* of the active unit */
            uint32_t twin                     = 0;
        };

        /* How the right eye treats a call of the application while a run is recorded (see GLRunRecorder::classify()). */
        enum class RunCallClass
        {
            FLUSH,    /* the run is played before the call is made */
            MATRIX,   /* changes a matrix: not kept, each draw of the run carries its matrices */
            NEUTRAL,  /* needs nothing */
            QUERY,    /* asks the driver for something: it is answered from the left eye, which is live */
            RECORDED  /* a state call that is kept and made again for the right eye */
        };

        /* What a recorded item of a run is. */
        enum class RunItemKind
        {
            ATTRIBUTE,
            DRAW,
            STATE
        };

        /* A block of the application that the right eye draws when the run is played, with what the eye needs of the moment the block ended. */
        struct RunDraw
        {
            uint32_t                first_call_index        = 0;     /* in the calls of the run */
            bool                    has_pushed_viewport     = false;
            uint32_t                mode                    = 0;
            std::array<float, 16>   modelview_f32mat        = {};    /* what the right eye loads: the eye matrix times the draw's own, for an affected draw */
            uint32_t                n_calls                 = 0;
            std::array<float, 16>   projection_f32mat       = {};
            std::array<int32_t, 4>  pushed_viewport_i32vec4 = {};    /* x, y, width, height for the right eye */
            std::array<int32_t, 4>  saved_viewport_i32vec4  = {};    /* what the viewport is set back to */
            TextureSelection        selection;
        };

        /* A state call that the right eye makes again. */
        struct RunStateCall
        {
            APIInterceptor::APIFunction function        = static_cast<APIInterceptor::APIFunction>(0);
            std::array<float, 4>        value_fp32vec4  = {};
            std::array<int32_t, 4>      value_i32vec4   = {};
        };

        /* One thing the right eye does when a run is played: where it is in the draws or the state calls. */
        struct RunItem
        {
            uint32_t    index = 0;
            RunItemKind kind  = RunItemKind::STATE;
        };
    }
}

#endif /* STEREO_CONVERTERDLL_OPENGL_TYPES_H */
