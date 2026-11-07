/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_analysis/frontends/shared/position_checker.h"
#include <math.h>

bool Analysis::APIDumpAnalysis::Frontends::PositionChecker::is_finite(const std::array<float, 3>& in_position)
{
    const bool is_x_finite = ::isfinite(in_position[0]);
    const bool is_y_finite = ::isfinite(in_position[1]);
    const bool is_z_finite = ::isfinite(in_position[2]);

    return (is_x_finite &&
            is_y_finite &&
            is_z_finite);
}
