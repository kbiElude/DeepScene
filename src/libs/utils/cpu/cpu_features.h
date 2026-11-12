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
        /* What the processor that the program runs on can do, for the code that is built for more than what every processor does. */
        class CpuFeatures
        {
        public:
            /* True if the processor has AVX2 and the system saves its registers when it switches threads. It is found out once. */
            static bool is_avx2_supported();
        };
    }
}

#endif /* UTILS_CPU_CPU_FEATURES_H */
