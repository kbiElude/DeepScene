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
        /* Whether rectangles together leave nothing of another rectangle uncovered: how the planner tells that the last few writes to a texture which the
         * frames fill a strip at a time have made all of it that anyone writes, so that the writes before them do not show.
         */
        class RectangleCoverage
        {
        public:
            /* True if every texel of the extent is in at least one of the rectangles (those which are not known, or have nothing in them, cover nothing); an
             * extent which is not known is covered by none.
             */
            static bool covers(const std::vector<Analysis::APIDumpAnalysis::AccessRectangle>& in_rectangle_vec,
                               const Analysis::APIDumpAnalysis::AccessRectangle&              in_extent);
        };
    }
}

#endif /* APIDUMPDEPENDENCIES_RECTANGLE_COVERAGE_H */
