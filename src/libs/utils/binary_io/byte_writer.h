/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Writes values as bytes, the same way on every machine: integers are little-endian, strings and vectors are prefixed with their
 * size.
 */
#ifndef UTILS_BINARYIO_BYTE_WRITER_H
#define UTILS_BINARYIO_BYTE_WRITER_H

#include <stddef.h>
#include <stdint.h>
#include <string>
#include <vector>

namespace Utils
{
    namespace BinaryIO
    {
        class ByteWriter
        {
        public:
            ByteWriter();

            const std::vector<uint8_t>& get_bytes() const;

            /* The bytes, without a copy. The writer is empty after it. */
            std::vector<uint8_t> take_bytes();

            void write_bool  (const bool&        in_value);
            void write_bytes (const void*        in_data_ptr,
                              const size_t&      in_n_bytes);
            void write_f32   (const float&       in_value);
            void write_string(const std::string& in_value);
            void write_u32   (const uint32_t&    in_value);
            void write_u64   (const uint64_t&    in_value);
            void write_u8    (const uint8_t&     in_value);

            /* Writes the size (u32), then the bytes. */
            void write_byte_vec(const std::vector<uint8_t>& in_value_vec);

        private:
            std::vector<uint8_t> m_byte_vec;
        };
    }
}

#endif /* UTILS_BINARYIO_BYTE_WRITER_H */
