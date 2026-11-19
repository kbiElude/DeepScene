/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "gl_texture_twin_table.h"
#include "common/macros.h"

namespace
{
    /* The array holds at most 1 MB. */
    constexpr uint32_t N_FLAT_NAMES = 1u << 18;
}

Stereo::ConverterDLL::GLTextureTwinTable::GLTextureTwinTable()
    :m_n_twins(0)
{
    /* Stub */
}

void Stereo::ConverterDLL::GLTextureTwinTable::erase(const uint32_t& in_texture)
{
    if (in_texture < N_FLAT_NAMES)
    {
        if (in_texture                <  m_flat_twin_vec.size() &&
            m_flat_twin_vec[in_texture] != 0)
        {
            m_flat_twin_vec[in_texture] = 0;

            m_n_twins--;
        }
    }
    else
    {
        m_n_twins -= m_large_twin_map.erase(in_texture);
    }
}

bool Stereo::ConverterDLL::GLTextureTwinTable::find(const uint32_t& in_texture,
                                                    uint32_t*       out_twin_ptr) const
{
    bool result = false;

    if (in_texture < N_FLAT_NAMES)
    {
        if (in_texture < m_flat_twin_vec.size() )
        {
            *out_twin_ptr = m_flat_twin_vec[in_texture];
            result        = (*out_twin_ptr != 0);
        }
    }
    else
    {
        const auto iterator = m_large_twin_map.find(in_texture);

        if (iterator != m_large_twin_map.end() )
        {
            *out_twin_ptr = iterator->second;
            result        = true;
        }
    }

    return result;
}

bool Stereo::ConverterDLL::GLTextureTwinTable::is_empty() const
{
    return (m_n_twins == 0);
}

void Stereo::ConverterDLL::GLTextureTwinTable::set(const uint32_t& in_texture,
                                                   const uint32_t& in_twin)
{
    AI_ASSERT_ALWAYS(in_twin != 0,
                     "0 is not the name of a texture");

    uint32_t old_twin = 0;

    if (find(in_texture,
            &old_twin) == false)
    {
        m_n_twins++;
    }

    if (in_texture < N_FLAT_NAMES)
    {
        if (in_texture >= m_flat_twin_vec.size() )
        {
            m_flat_twin_vec.resize(static_cast<size_t>(in_texture) + 1);
        }

        m_flat_twin_vec[in_texture] = in_twin;
    }
    else
    {
        m_large_twin_map[in_texture] = in_twin;
    }
}
