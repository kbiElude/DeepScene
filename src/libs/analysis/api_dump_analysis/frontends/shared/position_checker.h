/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef APIDUMPANALYSIS_FRONTENDS_POSITION_CHECKER_H
#define APIDUMPANALYSIS_FRONTENDS_POSITION_CHECKER_H

#include <array>

namespace Analysis
{
    namespace APIDumpAnalysis
    {
        namespace Frontends
        {
            class PositionChecker
            {
            public:
                /* Whether all three coordinates are numbers (not infinite, not NaN). */
                static bool is_finite(const std::array<float, 3>& in_position);
            };
        }
    }
}

#endif /* APIDUMPANALYSIS_FRONTENDS_POSITION_CHECKER_H */
