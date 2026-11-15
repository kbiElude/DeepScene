/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Translates the OpenGL texel formats into the formats of the project (see Utils::Graphics::TextureFormat) and describes how the bytes of an upload are laid out.
 */
#ifndef APIDUMPANALYSIS_FRONTENDS_OPENGL_GL_TEXTURE_FORMAT_MAPPER_H
#define APIDUMPANALYSIS_FRONTENDS_OPENGL_GL_TEXTURE_FORMAT_MAPPER_H

#include "utils/graphics/texture_format.h"
#include <stdint.h>

namespace Analysis
{
    namespace APIDumpAnalysis
    {
        namespace Frontends
        {
            class GLTextureFormatMapper
            {
            public:
                /* Returns the size in bytes of one texel in an upload of the given format and type. For a type that packs the channels into one word, this
                 * is the size of the word. Returns 0 for a format and type pair that is not followed.
                 */
                static uint32_t get_n_bytes_per_texel(const uint32_t& in_format,
                                                      const uint32_t& in_type);

                /* Returns the format of a level created with the given internal format. An internal format that does not specify the size of a channel
                 * (GL_RGBA) gets what drivers typically choose, 8 bits per channel. A format that is not followed gives UNKNOWN.
                 */
                static Utils::Graphics::TextureFormat get_texture_format(const uint32_t& in_internal_format);

                /* Returns the distance in bytes from one row of an upload to the next. A row is as long as the row length says (the width if the row
                 * length is 0), rounded up to a multiple of the alignment (GL_UNPACK_ALIGNMENT, which is 1, 2, 4 or 8). Returns 0 if the texel size is unknown.
                 */
                static uint32_t get_unpack_row_pitch(const uint32_t& in_width,
                                                     const uint32_t& in_n_bytes_per_texel,
                                                     const uint32_t& in_row_length,
                                                     const uint32_t& in_alignment);
            };
        }
    }
}

#endif /* APIDUMPANALYSIS_FRONTENDS_OPENGL_GL_TEXTURE_FORMAT_MAPPER_H */
