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
        /* The XXH64 stripe loop for processors with AVX2. It is a class of its own because its file is built with AVX2 enabled, so the file must not
         * contain anything that the rest of the program may call.
         */
        class HasherAvx2
        {
        public:
            /* Updates the four XXH64 accumulators with in_n_stripes stripes of 32 bytes each. The primes are the first and second primes of XXH64. */
            static void hash_stripes(const uint8_t*  in_data_ptr,
                                     const size_t&   in_n_stripes,
                                     const uint64_t& in_prime_1,
                                     const uint64_t& in_prime_2,
                                     uint64_t*       io_accumulator_u64vec4);
        };
    }
}

#endif /* UTILS_HASHING_HASHER_AVX2_H */
