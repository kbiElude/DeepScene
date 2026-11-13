/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * What OpenGL calls the texels of a texture, in the words of the project (see Utils::Graphics::TextureFormat), and how the bytes of an upload are laid out.
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
                /* The bytes of a texel that an upload of the format and the type has (the type that packs the channels in a word gives the word); 0 for a pair that is
                 * not followed.
                 */
                static uint32_t get_n_bytes_per_texel(const uint32_t& in_format,
                                                      const uint32_t& in_type);

                /* The format of a level that is made with the internal format. An internal format that does not say the size of a channel (GL_RGBA) is what the drivers
                 * make of it, 8 bits for a channel; the one that is not followed is UNKNOWN.
                 */
                static Utils::Graphics::TextureFormat get_texture_format(const uint32_t& in_internal_format);

                /* The distance in bytes from a row of an upload to the next: the row as long as the row length says (the width, if it is 0), up to a multiple of the
                 * alignment (GL_UNPACK_ALIGNMENT, which is 1, 2, 4 or 8). 0 if the texel has no known size.
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
