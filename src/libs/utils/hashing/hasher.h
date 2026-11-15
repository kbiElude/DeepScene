/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Hashes values the same way on every machine and in every run. Integers are taken as little-endian, floats by their bits
 * (-0 is hashed as 0 and all NaNs as one value), and data of variable size is hashed together with its size. Match keys are
 * built with it and written to the analysis file, so how it treats a value must never change.
 *
 * A hash and a key are both 64 bits but mean different things. A hash is the digest of bytes: the contents of a resource,
 * the code of a shader, the start of a file (from hash_bytes(), or from a Hasher that was fed those bytes). A key identifies a
 * structure: it comes from a Hasher that was fed the values that make a pass, a draw or a resource what it is (see KeyBuilder).
 * A key can contain hashes. For example, the key of a resource contains the hash of its contents.
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

            /* Values are hashed in the order they are added. */
            void add_bool(const bool& in_value);

            void     add_bytes (const void*        in_data_ptr,
                                const size_t&      in_n_bytes);
            void     add_f32   (const float&       in_value);
            void     add_string(const std::string& in_value);
            void     add_u32   (const uint32_t&    in_value);
            void     add_u64   (const uint64_t&    in_value);
            void     add_u8    (const uint8_t&     in_value);
            uint64_t get_digest() const;

            /* Hashes bytes that are not part of a key, such as the contents of a resource or the code of a shader. Fast for big data. */
            static uint64_t hash_bytes(const void*   in_data_ptr,
                                       const size_t& in_n_bytes);

        private:
            /* Up to this many bytes are stored inside the object. This is almost always enough because a key is made of a few values. */
            static constexpr size_t N_INLINE_BYTES = 2048;

            void append(const void*   in_data_ptr,
                        const size_t& in_n_bytes);

            /* Returns the same hash as hash_bytes(). The 32-byte stripes are processed by HasherAvx2::hash_stripes(), which is faster than the scalar
             * code for more than a few dozen bytes on a processor with AVX2. The digest is identical for every input.
             */
            static uint64_t hash_bytes_avx2(const void*   in_data_ptr,
                                            const size_t& in_n_bytes);

            /* Inputs shorter than this use the scalar code because the AVX2 stripes would not pay off. */
            static const size_t N_MIN_AVX2_BYTES;

            bool                 m_is_spilled;
            uint8_t              m_inline_u8vec2048[N_INLINE_BYTES];
            size_t               m_n_inline_bytes;
            std::vector<uint8_t> m_spill_u8_vec; /* all bytes, once there are too many for the inline storage */
        };
    }
}

#endif /* UTILS_HASHING_HASHER_H */
