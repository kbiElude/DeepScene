/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "utils/cpu/cpu_features.h"
#include <immintrin.h>
#include <intrin.h>

bool Utils::Cpu::CpuFeatures::is_avx2_supported()
{
    /* Detected on the first call. The initialization of a local static is thread safe, and every later call returns the same value. */
    static const bool result = []()
    {
        constexpr int CPUID_EXTENDED_LEAF   = 7;
        constexpr int CPUID_FEATURES_LEAF   = 1;
        constexpr int EBX_AVX2_BIT          = 5;
        constexpr int ECX_AVX_BIT           = 28;
        constexpr int ECX_OSXSAVE_BIT       = 27;
        constexpr int N_CPUID_REGISTERS     = 4;
        constexpr int REGISTER_EBX          = 1;
        constexpr int REGISTER_ECX          = 2;
        constexpr int XCR0_SSE_AND_AVX_BITS = 6; /* the operating system saves the SSE and AVX registers when it switches threads */

        int  cpu_info_array[N_CPUID_REGISTERS] = {};
        bool is_supported                      = false;

        __cpuid(cpu_info_array,
                0);

        if (cpu_info_array[0] >= CPUID_EXTENDED_LEAF)
        {
            int features_array[N_CPUID_REGISTERS] = {};
            int extended_array[N_CPUID_REGISTERS] = {};

            __cpuidex(features_array,
                      CPUID_FEATURES_LEAF,
                      0);
            __cpuidex(extended_array,
                      CPUID_EXTENDED_LEAF,
                      0);

            const bool has_avx     = ( (features_array[REGISTER_ECX] >> ECX_AVX_BIT)     & 1) != 0;
            const bool has_avx2    = ( (extended_array[REGISTER_EBX] >> EBX_AVX2_BIT)    & 1) != 0;
            const bool has_osxsave = ( (features_array[REGISTER_ECX] >> ECX_OSXSAVE_BIT) & 1) != 0;

            if (has_avx     &&
                has_avx2    &&
                has_osxsave)
            {
                const unsigned long long enabled_state = _xgetbv(0);

                is_supported = ( (enabled_state & XCR0_SSE_AND_AVX_BITS) == XCR0_SSE_AND_AVX_BITS);
            }
        }

        return is_supported;
    }();

    return result;
}
