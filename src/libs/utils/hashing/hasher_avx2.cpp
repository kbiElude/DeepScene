/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * This file is built for AVX2 (see the CMakeLists.txt of the library), so that nothing in it may be used on a processor that does not have it: Hasher calls it only after it
 * found out that the processor has AVX2. It is kept to what has to be here: the stripes of XXH64, with no function of the standard library and none that is shared with other
 * files (such a function is built here with AVX2, and the linker may pick this copy of it for the callers that are not).
 */
#include "utils/hashing/hasher_avx2.h"
#include <immintrin.h>

void Utils::Hashing::HasherAvx2::hash_stripes(const uint8_t*  in_data_ptr,
                                              const size_t&   in_n_stripes,
                                              const uint64_t& in_prime_1,
                                              const uint64_t& in_prime_2,
                                              uint64_t*       io_accumulator_u64vec4)
{
    constexpr int N_BITS_PER_HALF      = 32;
    constexpr int N_BITS_ROTATED       = 31;
    constexpr int N_BITS_ROTATED_OTHER = 64 - N_BITS_ROTATED;

    /* The four accumulators of XXH64 are the four 64-bit lanes of a register, and a stripe of 32 bytes is the four 64-bit words that go to them, so that each step of XXH64
     * (add the word times the second prime, rotate by 31, times the first prime) is the same step for the four lanes. AVX2 has no multiplication of 64 bits by 64: it is made of the
     * ones of 32 by 32 that it has, as the low half of the product is the sum of the low product and the two cross products shifted up by 32 bits. The lanes wrap round at 64 bits
     * as the numbers of XXH64 do, so that what comes out is what XXH64 gives.
     */
    const __m256i prime_1_v      = _mm256_set1_epi64x(static_cast<long long>(in_prime_1) );
    const __m256i prime_1_high_v = _mm256_srli_epi64(prime_1_v,
                                                     N_BITS_PER_HALF);
    const __m256i prime_2_v      = _mm256_set1_epi64x(static_cast<long long>(in_prime_2) );
    const __m256i prime_2_high_v = _mm256_srli_epi64(prime_2_v,
                                                     N_BITS_PER_HALF);

    __m256i accumulator_v = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(io_accumulator_u64vec4) );

    for (size_t n_stripe = 0;
                n_stripe < in_n_stripes;
              ++n_stripe)
    {
        const __m256i word_v = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(in_data_ptr + n_stripe * sizeof(__m256i) ) );

        /* word * prime 2 */
        const __m256i word_low_v   = _mm256_mul_epu32(word_v,
                                                      prime_2_v);
        const __m256i word_cross_v = _mm256_add_epi64(_mm256_mul_epu32(_mm256_srli_epi64(word_v,
                                                                                         N_BITS_PER_HALF),
                                                                       prime_2_v),
                                                      _mm256_mul_epu32(word_v,
                                                                       prime_2_high_v) );
        const __m256i word_times_v = _mm256_add_epi64(word_low_v,
                                                      _mm256_slli_epi64(word_cross_v,
                                                                        N_BITS_PER_HALF) );

        /* acc = rotate(acc + word * prime 2, 31) */
        const __m256i sum_v     = _mm256_add_epi64(accumulator_v,
                                                   word_times_v);
        const __m256i rotated_v = _mm256_or_si256(_mm256_slli_epi64(sum_v,
                                                                    N_BITS_ROTATED),
                                                  _mm256_srli_epi64(sum_v,
                                                                    N_BITS_ROTATED_OTHER) );

        /* acc = acc * prime 1 */
        const __m256i rotated_low_v   = _mm256_mul_epu32(rotated_v,
                                                         prime_1_v);
        const __m256i rotated_cross_v = _mm256_add_epi64(_mm256_mul_epu32(_mm256_srli_epi64(rotated_v,
                                                                                            N_BITS_PER_HALF),
                                                                          prime_1_v),
                                                         _mm256_mul_epu32(rotated_v,
                                                                          prime_1_high_v) );

        accumulator_v = _mm256_add_epi64(rotated_low_v,
                                         _mm256_slli_epi64(rotated_cross_v,
                                                           N_BITS_PER_HALF) );
    }

    _mm256_storeu_si256(reinterpret_cast<__m256i*>(io_accumulator_u64vec4),
                        accumulator_v);
}
