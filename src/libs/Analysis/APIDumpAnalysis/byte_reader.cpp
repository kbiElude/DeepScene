/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "Analysis/APIDumpAnalysis/byte_reader.h"
#include <string.h>

Analysis::APIDumpAnalysis::ByteReader::ByteReader(const uint8_t* in_data_ptr,
                                                  const size_t&  in_n_bytes)
    :m_data_ptr  (in_data_ptr),
     m_has_failed(false),
     m_n_bytes   (in_n_bytes),
     m_position  (0)
{
    /* Stub */
}

void Analysis::APIDumpAnalysis::ByteReader::fail()
{
    m_has_failed = true;
}

size_t Analysis::APIDumpAnalysis::ByteReader::get_n_bytes_left() const
{
    return m_n_bytes - m_position;
}

bool Analysis::APIDumpAnalysis::ByteReader::has_failed() const
{
    return m_has_failed;
}

bool Analysis::APIDumpAnalysis::ByteReader::is_at_end() const
{
    return (m_position == m_n_bytes);
}

bool Analysis::APIDumpAnalysis::ByteReader::read_bool()
{
    return (read_u8() != 0);
}

std::vector<uint8_t> Analysis::APIDumpAnalysis::ByteReader::read_byte_vec()
{
    std::vector<uint8_t> result_vec(read_count(sizeof(uint8_t) ));

    read_bytes(result_vec.data(),
               result_vec.size() );

    return result_vec;
}

void Analysis::APIDumpAnalysis::ByteReader::read_bytes(void*         out_data_ptr,
                                                       const size_t& in_n_bytes)
{
    if (get_n_bytes_left() < in_n_bytes)
    {
        m_has_failed = true;
        m_position   = m_n_bytes;

        ::memset(out_data_ptr,
                 0,
                 in_n_bytes);
    }
    else
    if (in_n_bytes != 0)
    {
        ::memcpy(out_data_ptr,
                 m_data_ptr + m_position,
                 in_n_bytes);

        m_position += in_n_bytes;
    }
}

uint32_t Analysis::APIDumpAnalysis::ByteReader::read_count(const size_t& in_min_item_n_bytes)
{
    uint32_t result = read_u32();

    /* A damaged count must not ask for gigabytes. */
    if (static_cast<uint64_t>(result) * in_min_item_n_bytes > get_n_bytes_left() )
    {
        m_has_failed = true;
        result       = 0;
    }

    return result;
}

float Analysis::APIDumpAnalysis::ByteReader::read_f32()
{
    const uint32_t bits   = read_u32();
    float          result = 0.0f;

    ::memcpy(&result,
             &bits,
              sizeof(result) );

    return result;
}

std::string Analysis::APIDumpAnalysis::ByteReader::read_string()
{
    const uint32_t n_chars = read_count(sizeof(char) );
    std::string    result(n_chars,
                          '\0');

    read_bytes(&result[0],
               result.size() );

    return result;
}

uint32_t Analysis::APIDumpAnalysis::ByteReader::read_u32()
{
    constexpr size_t N_BITS_PER_BYTE = 8;

    uint8_t  byte_u8vec4[sizeof(uint32_t)] = {};
    uint32_t result                        = 0;

    /* A read past the end gives zeros. */
    read_bytes(byte_u8vec4,
               sizeof(byte_u8vec4) );

    for (size_t n_byte = 0;
                n_byte < sizeof(byte_u8vec4);
              ++n_byte)
    {
        result |= static_cast<uint32_t>(byte_u8vec4[n_byte]) << (n_byte * N_BITS_PER_BYTE);
    }

    return result;
}

uint64_t Analysis::APIDumpAnalysis::ByteReader::read_u64()
{
    constexpr size_t N_BITS_PER_BYTE = 8;

    uint8_t  byte_u8vec8[sizeof(uint64_t)] = {};
    uint64_t result                        = 0;

    read_bytes(byte_u8vec8,
               sizeof(byte_u8vec8) );

    for (size_t n_byte = 0;
                n_byte < sizeof(byte_u8vec8);
              ++n_byte)
    {
        result |= static_cast<uint64_t>(byte_u8vec8[n_byte]) << (n_byte * N_BITS_PER_BYTE);
    }

    return result;
}

uint8_t Analysis::APIDumpAnalysis::ByteReader::read_u8()
{
    uint8_t result = 0;

    read_bytes(&result,
               sizeof(result) );

    return result;
}
