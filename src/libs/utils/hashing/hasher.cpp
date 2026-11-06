/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "utils/hashing/hasher.h"
#include <math.h>
#include <string.h>

/* The xxHash which Zstandard comes with, built into this file only. */
#define XXH_INLINE_ALL
#include "xxhash.h"

Utils::Hashing::Hasher::Hasher()
    :m_is_spilled    (false),
     m_n_inline_bytes(0)
{
    /* Stub */
}

void Utils::Hashing::Hasher::add_bool(const bool& in_value)
{
    add_u8(static_cast<uint8_t>(in_value) );
}

void Utils::Hashing::Hasher::add_bytes(const void*   in_data_ptr,
                                       const size_t& in_n_bytes)
{
    /* The size says where the bytes end, so that ("ab", "c") and ("a", "bc") differ. */
    add_u64(static_cast<uint64_t>(in_n_bytes) );

    append(in_data_ptr,
           in_n_bytes);
}

void Utils::Hashing::Hasher::add_f32(const float& in_value)
{
    constexpr uint32_t NAN_BITS = 0x7FC00000;
    uint32_t           bits     = 0;

    if (::isnan(in_value) )
    {
        bits = NAN_BITS;
    }
    else
    if (in_value != 0.0f)
    {
        ::memcpy(&bits,
                 &in_value,
                  sizeof(bits) );
    }

    add_u32(bits);
}

void Utils::Hashing::Hasher::add_string(const std::string& in_value)
{
    add_bytes(in_value.data(),
              in_value.size() );
}

void Utils::Hashing::Hasher::add_u32(const uint32_t& in_value)
{
    constexpr size_t N_BITS_PER_BYTE              = 8;
    uint8_t          byte_u8vec4[sizeof(in_value)];

    for (size_t n_byte = 0;
                n_byte < sizeof(in_value);
              ++n_byte)
    {
        byte_u8vec4[n_byte] = static_cast<uint8_t>(in_value >> (n_byte * N_BITS_PER_BYTE) );
    }

    append(byte_u8vec4,
           sizeof(byte_u8vec4) );
}

void Utils::Hashing::Hasher::add_u64(const uint64_t& in_value)
{
    constexpr size_t N_BITS_PER_BYTE               = 8;
    uint8_t          byte_u8vec8[sizeof(in_value)];

    for (size_t n_byte = 0;
                n_byte < sizeof(in_value);
              ++n_byte)
    {
        byte_u8vec8[n_byte] = static_cast<uint8_t>(in_value >> (n_byte * N_BITS_PER_BYTE) );
    }

    append(byte_u8vec8,
           sizeof(byte_u8vec8) );
}

void Utils::Hashing::Hasher::add_u8(const uint8_t& in_value)
{
    append(&in_value,
           sizeof(in_value) );
}

void Utils::Hashing::Hasher::append(const void*   in_data_ptr,
                                    const size_t& in_n_bytes)
{
    const uint8_t* data_u8_ptr = static_cast<const uint8_t*>(in_data_ptr);

    if (m_is_spilled                  == false          &&
        m_n_inline_bytes + in_n_bytes >  N_INLINE_BYTES)
    {
        m_spill_u8_vec.assign(m_inline_u8vec2048,
                              m_inline_u8vec2048 + m_n_inline_bytes);

        m_is_spilled = true;
    }

    if (m_is_spilled)
    {
        m_spill_u8_vec.insert(m_spill_u8_vec.end(),
                              data_u8_ptr,
                              data_u8_ptr + in_n_bytes);
    }
    else
    {
        ::memcpy(m_inline_u8vec2048 + m_n_inline_bytes,
                 data_u8_ptr,
                 in_n_bytes);

        m_n_inline_bytes += in_n_bytes;
    }
}

uint64_t Utils::Hashing::Hasher::get_digest() const
{
    return (m_is_spilled) ? hash_bytes(m_spill_u8_vec.data(),
                                       m_spill_u8_vec.size() )
                          : hash_bytes(m_inline_u8vec2048,
                                       m_n_inline_bytes);
}

uint64_t Utils::Hashing::Hasher::hash_bytes(const void*   in_data_ptr,
                                            const size_t& in_n_bytes)
{
    constexpr uint64_t SEED = 0;

    return XXH64(in_data_ptr,
                 in_n_bytes,
                 SEED);
}
