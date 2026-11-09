/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Tests for RectangleCoverage: whether rectangles leave nothing of another uncovered.
 */
#include <gtest/gtest.h>
#include "analysis/api_dump_dependencies/rectangle_coverage.h"

namespace
{
    using Analysis::APIDumpAnalysis::AccessRectangle;
    using Analysis::APIDumpDependencies::RectangleCoverage;

    AccessRectangle make_rectangle(const uint32_t& in_x,
                                   const uint32_t& in_y,
                                   const uint32_t& in_width,
                                   const uint32_t& in_height,
                                   const bool&     in_is_known = true)
    {
        AccessRectangle result;

        result.height   = in_height;
        result.is_known = in_is_known;
        result.width    = in_width;
        result.x        = in_x;
        result.y        = in_y;

        return result;
    }
}

TEST(RectangleCoverage, StripsThatMeetCoverTheExtentAndStripsWithAGapDoNot)
{
    const AccessRectangle extent = make_rectangle(0, 0, 40, 10);

    EXPECT_TRUE (RectangleCoverage::covers({make_rectangle(0, 0, 10, 10), make_rectangle(10, 0, 10, 10), make_rectangle(20, 0, 10, 10), make_rectangle(30, 0, 10, 10)},
                                           extent) );
    EXPECT_TRUE (RectangleCoverage::covers({make_rectangle(30, 0, 10, 10), make_rectangle(0, 0, 10, 10), make_rectangle(20, 0, 10, 10), make_rectangle(10, 0, 10, 10)},
                                           extent) ); /* in any order */
    EXPECT_FALSE(RectangleCoverage::covers({make_rectangle(0, 0, 10, 10), make_rectangle(10, 0, 10, 10), make_rectangle(30, 0, 10, 10)},
                                           extent) );
    EXPECT_FALSE(RectangleCoverage::covers({make_rectangle(0, 0, 10, 10), make_rectangle(11, 0, 29, 10)},
                                           extent) ); /* a texel between them */
}

TEST(RectangleCoverage, RectanglesThatOverlapOrAreLargerThanTheExtentCoverIt)
{
    const AccessRectangle extent = make_rectangle(5, 5, 20, 20);

    EXPECT_TRUE(RectangleCoverage::covers({make_rectangle(0, 0, 100, 100)},
                                          extent) );
    EXPECT_TRUE(RectangleCoverage::covers({make_rectangle(0, 0, 15, 40), make_rectangle(10, 0, 40, 40)},
                                          extent) );
}

TEST(RectangleCoverage, TheRowsOfAnExtentAreCoveredEachOnItsOwn)
{
    const AccessRectangle extent = make_rectangle(0, 0, 20, 20);

    EXPECT_TRUE (RectangleCoverage::covers({make_rectangle(0, 0, 20, 10), make_rectangle(0, 10, 10, 10), make_rectangle(10, 10, 10, 10)},
                                           extent) );
    EXPECT_FALSE(RectangleCoverage::covers({make_rectangle(0, 0, 20, 10), make_rectangle(0, 10, 10, 10), make_rectangle(10, 11, 10, 9)},
                                           extent) );
}

TEST(RectangleCoverage, ARectangleThatIsNotKnownOrHasNothingInItCoversNothing)
{
    const AccessRectangle extent = make_rectangle(0, 0, 10, 10);

    EXPECT_FALSE(RectangleCoverage::covers({},
                                           extent) );
    EXPECT_FALSE(RectangleCoverage::covers({make_rectangle(0, 0, 10, 10, false /* in_is_known */)},
                                           extent) );
    EXPECT_FALSE(RectangleCoverage::covers({make_rectangle(0, 0, 0, 10), make_rectangle(0, 0, 10, 0)},
                                           extent) );
}

TEST(RectangleCoverage, AnExtentThatIsNotKnownIsCoveredByNone)
{
    EXPECT_FALSE(RectangleCoverage::covers({make_rectangle(0, 0, 100, 100)},
                                           make_rectangle(0, 0, 10, 10, false /* in_is_known */) ) );
    EXPECT_FALSE(RectangleCoverage::covers({make_rectangle(0, 0, 100, 100)},
                                           make_rectangle(0, 0, 0, 0) ) );
}
