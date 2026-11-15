/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef APIDUMPDEPENDENCIES_RECTANGLE_COVERAGE_H
#define APIDUMPDEPENDENCIES_RECTANGLE_COVERAGE_H

#include "analysis/api_dump_analysis/types.h"
#include <vector>

namespace Analysis
{
    namespace APIDumpDependencies
    {
        /* Tests whether a set of rectangles leaves any part of another rectangle uncovered. The planner uses it to tell that the last few writes to
         * a texture, which frames fill one strip at a time, have written everything that any frame writes, so that the earlier writes no longer show.
         */
        class RectangleCoverage
        {
        public:
            /* True if every texel of the extent lies in at least one of the rectangles. Rectangles that are unknown or empty cover nothing. An extent
             * that is unknown or empty is never covered.
             */
            static bool covers(const std::vector<Analysis::APIDumpAnalysis::AccessRectangle>& in_rectangle_vec,
                               const Analysis::APIDumpAnalysis::AccessRectangle&              in_extent);
        };
    }
}

#endif /* APIDUMPDEPENDENCIES_RECTANGLE_COVERAGE_H */
