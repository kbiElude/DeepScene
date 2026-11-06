/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Reads values from bytes written by ByteWriter. A read past the end returns 0 and marks the reader as failed, so callers can
 * check once at the end.
 */
#ifndef UTILS_BINARYIO_BYTE_READER_H
#define UTILS_BINARYIO_BYTE_READER_H

#include <stddef.h>
#include <stdint.h>
#include <string>
#include <vector>

namespace Utils
{
    namespace BinaryIO
    {
        class ByteReader
        {
        public:
            /* The bytes must outlive the reader. */
            ByteReader(const uint8_t* in_data_ptr,
                       const size_t&  in_n_bytes);

            /* Makes the reader fail: for what was read, and cannot be right. */
            void fail();

            size_t get_n_bytes_left() const;

            /* True if a read went past the end (it gave zeros), or a count was impossible. */
            bool has_failed() const;

            bool                 is_at_end    () const;
            bool                 read_bool    ();
            std::vector<uint8_t> read_byte_vec();
            void                 read_bytes   (void*         out_data_ptr,
                                               const size_t& in_n_bytes);

            /* Reads an item count; fails the reader and returns 0 if the items cannot fit in the remaining bytes. */
            uint32_t read_count(const size_t& in_min_item_n_bytes);

            float       read_f32   ();
            std::string read_string();
            uint32_t    read_u32   ();
            uint64_t    read_u64   ();
            uint8_t     read_u8    ();

        private:
            const uint8_t* m_data_ptr;
            bool           m_has_failed;
            size_t         m_n_bytes;
            size_t         m_position;
        };
    }
}

#endif /* UTILS_BINARYIO_BYTE_READER_H */
