/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Hashes values the same way on every machine and in every run: integers are taken as little-endian, floats by their
 * bits (with -0 as 0, and all NaNs as one), and what has a variable size comes with its size. Match keys are built with
 * it, and are written to the analysis file, so what it does with a value must never change.
 *
 * A hash and a key are both 64 bits, and are not the same thing. A hash is the digest of bytes: the contents of a resource,
 * the code of a shader, the start of a file (hash_bytes(), or a Hasher which was given those bytes). A key is the identity of a
 * structure: a Hasher which was given the values that make a pass, a draw or a resource what it is (see KeyBuilder). A key can
 * contain hashes, as the key of a resource contains the hash of its contents.
 */
#ifndef UTILS_HASHING_HASHER_H
#define UTILS_HASHING_HASHER_H

#include <stddef.h>
#include <stdint.h>
#include <string>
#include <vector>

namespace Utils
{
    namespace Hashing
    {
        class Hasher
        {
        public:
            Hasher();

            /* The values are taken in the order they are added in. */
            void add_bool(const bool& in_value);

            void     add_bytes (const void*        in_data_ptr,
                                const size_t&      in_n_bytes);
            void     add_f32   (const float&       in_value);
            void     add_string(const std::string& in_value);
            void     add_u32   (const uint32_t&    in_value);
            void     add_u64   (const uint64_t&    in_value);
            void     add_u8    (const uint8_t&     in_value);
            uint64_t get_digest() const;

            /* Hashes bytes which are not part of a key: the contents of a resource, the code of a shader. Fast, for big data. */
            static uint64_t hash_bytes(const void*   in_data_ptr,
                                       const size_t& in_n_bytes);

        private:
            /* Up to this many bytes are kept in the object, which is nearly always the case: a key is made of a few values. */
            static constexpr size_t N_INLINE_BYTES = 2048;

            void append(const void*   in_data_ptr,
                        const size_t& in_n_bytes);

            bool                 m_is_spilled;
            uint8_t              m_inline_u8vec2048[N_INLINE_BYTES];
            size_t               m_n_inline_bytes;
            std::vector<uint8_t> m_spill_u8_vec; /* all the bytes, once they were too many for the inline ones */
        };
    }
}

#endif /* UTILS_HASHING_HASHER_H */
