/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef UTILS_HASHING_HASHER_AVX2_H
#define UTILS_HASHING_HASHER_AVX2_H

#include <stddef.h>
#include <stdint.h>

namespace Utils
{
    namespace Hashing
    {
        /* The stripes of XXH64 for a processor that has AVX2. It is a class of its own because its file is built for AVX2, and so is not to be the home of anything that the rest
         * of the program may call.
         */
        class HasherAvx2
        {
        public:
            /* The four accumulators of XXH64 (the arguments are its first and second prime) after the stripes of 32 bytes that in_n_stripes of them are. */
            static void hash_stripes(const uint8_t*  in_data_ptr,
                                     const size_t&   in_n_stripes,
                                     const uint64_t& in_prime_1,
                                     const uint64_t& in_prime_2,
                                     uint64_t*       io_accumulator_u64vec4);
        };
    }
}

#endif /* UTILS_HASHING_HASHER_AVX2_H */
