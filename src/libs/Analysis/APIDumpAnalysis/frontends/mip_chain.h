/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef APIDUMPANALYSIS_FRONTENDS_MIP_CHAIN_H
#define APIDUMPANALYSIS_FRONTENDS_MIP_CHAIN_H

#include <stdint.h>

namespace Analysis
{
    namespace APIDumpAnalysis
    {
        namespace Frontends
        {
            class MipChain
            {
            public:
                /* The number of mip levels of a texture; 0 asked for means all down to 1x1. */
                static uint32_t get_level_count(const uint32_t& in_n_levels_asked,
                                                const uint32_t& in_width,
                                                const uint32_t& in_height);
            };
        }
    }
}

#endif /* APIDUMPANALYSIS_FRONTENDS_MIP_CHAIN_H */
