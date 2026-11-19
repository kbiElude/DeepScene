/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * The twin that each texture of the application has for the right eye. The names of textures are small numbers, so the names below a limit are looked up in an
 * array, and the few above it in a map: a lookup is made for each texture of each draw.
 */
#ifndef STEREO_CONVERTERDLL_GL_TEXTURE_TWIN_TABLE_H
#define STEREO_CONVERTERDLL_GL_TEXTURE_TWIN_TABLE_H

#include <map>
#include <stdint.h>
#include <vector>

namespace Stereo
{
    namespace ConverterDLL
    {
        class GLTextureTwinTable
        {
        public:
            GLTextureTwinTable();

            /* Forgets the twin of the texture, if it has one. */
            void erase(const uint32_t& in_texture);

            /* True, and the name of the twin, if the texture has one. */
            bool find(const uint32_t& in_texture,
                      uint32_t*       out_twin_ptr) const;

            bool is_empty() const;

            /* The twin must not be 0, which is not the name of a texture. */
            void set(const uint32_t& in_texture,
                     const uint32_t& in_twin);

        private:
            std::vector<uint32_t>        m_flat_twin_vec; /* indexed by the name of the texture; 0: no twin */
            std::map<uint32_t, uint32_t> m_large_twin_map; /* the textures whose names are too large for the array */
            size_t                       m_n_twins;
        };
    }
}

#endif /* STEREO_CONVERTERDLL_GL_TEXTURE_TWIN_TABLE_H */
