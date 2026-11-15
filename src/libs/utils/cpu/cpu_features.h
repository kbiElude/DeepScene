/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef UTILS_CPU_CPU_FEATURES_H
#define UTILS_CPU_CPU_FEATURES_H

namespace Utils
{
    namespace Cpu
    {
        /* Reports which optional instruction sets the processor supports, for code that is built to use them. */
        class CpuFeatures
        {
        public:
            /* True if the processor supports AVX2 and the operating system saves the AVX registers when it switches threads. The result is detected once. */
            static bool is_avx2_supported();
        };
    }
}

#endif /* UTILS_CPU_CPU_FEATURES_H */
