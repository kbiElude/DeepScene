/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "Analysis/APIDumpAnalysis/frontends/mip_chain.h"
#include <algorithm>

uint32_t Analysis::APIDumpAnalysis::Frontends::MipChain::get_level_count(const uint32_t& in_n_levels_asked,
                                                                         const uint32_t& in_width,
                                                                         const uint32_t& in_height)
{
    uint32_t result = in_n_levels_asked;

    if (in_n_levels_asked == 0)
    {
        uint32_t extent = std::max<uint32_t>(std::max<uint32_t>(in_width,
                                                                in_height),
                                             1);

        result = 1;

        while (extent > 1)
        {
            extent >>= 1;
            result++;
        }
    }

    return result;
}
