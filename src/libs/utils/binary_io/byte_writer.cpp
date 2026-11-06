/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "utils/binary_io/byte_writer.h"
#include <string.h>

Utils::BinaryIO::ByteWriter::ByteWriter()
{
    /* Stub */
}

const std::vector<uint8_t>& Utils::BinaryIO::ByteWriter::get_bytes() const
{
    return m_byte_vec;
}

std::vector<uint8_t> Utils::BinaryIO::ByteWriter::take_bytes()
{
    return std::move(m_byte_vec);
}

void Utils::BinaryIO::ByteWriter::write_bool(const bool& in_value)
{
    write_u8(static_cast<uint8_t>(in_value) );
}

void Utils::BinaryIO::ByteWriter::write_byte_vec(const std::vector<uint8_t>& in_value_vec)
{
    write_u32(static_cast<uint32_t>(in_value_vec.size() ));
    write_bytes(in_value_vec.data(),
                in_value_vec.size() );
}

void Utils::BinaryIO::ByteWriter::write_bytes(const void*   in_data_ptr,
                                              const size_t& in_n_bytes)
{
    const uint8_t* data_u8_ptr = static_cast<const uint8_t*>(in_data_ptr);

    m_byte_vec.insert(m_byte_vec.end(),
                      data_u8_ptr,
                      data_u8_ptr + in_n_bytes);
}

void Utils::BinaryIO::ByteWriter::write_f32(const float& in_value)
{
    uint32_t bits = 0;

    ::memcpy(&bits,
             &in_value,
              sizeof(bits) );

    write_u32(bits);
}

void Utils::BinaryIO::ByteWriter::write_string(const std::string& in_value)
{
    write_u32  (static_cast<uint32_t>(in_value.size() ));
    write_bytes(in_value.data(),
                in_value.size() );
}

void Utils::BinaryIO::ByteWriter::write_u32(const uint32_t& in_value)
{
    constexpr size_t N_BITS_PER_BYTE = 8;

    for (size_t n_byte = 0;
                n_byte < sizeof(in_value);
              ++n_byte)
    {
        write_u8(static_cast<uint8_t>(in_value >> (n_byte * N_BITS_PER_BYTE) ));
    }
}

void Utils::BinaryIO::ByteWriter::write_u64(const uint64_t& in_value)
{
    constexpr size_t N_BITS_PER_BYTE = 8;

    for (size_t n_byte = 0;
                n_byte < sizeof(in_value);
              ++n_byte)
    {
        write_u8(static_cast<uint8_t>(in_value >> (n_byte * N_BITS_PER_BYTE) ));
    }
}

void Utils::BinaryIO::ByteWriter::write_u8(const uint8_t& in_value)
{
    m_byte_vec.push_back(in_value);
}
