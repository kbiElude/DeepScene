/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_dependencies/rectangle_coverage.h"
#include <algorithm>
#include <stdint.h>
#include <utility>

bool Analysis::APIDumpDependencies::RectangleCoverage::covers(const std::vector<Analysis::APIDumpAnalysis::AccessRectangle>& in_rectangle_vec,
                                                              const Analysis::APIDumpAnalysis::AccessRectangle&              in_extent)
{
    /* A rectangle as its edges, clipped to the extent; the right and the top are the first texel not in it. */
    struct Edges
    {
        uint64_t bottom;
        uint64_t left;
        uint64_t right;
        uint64_t top;
    };

    std::vector<Edges>    edges_vec;
    const uint64_t        extent_bottom = in_extent.y;
    const uint64_t        extent_left   = in_extent.x;
    const uint64_t        extent_right  = static_cast<uint64_t>(in_extent.x) + in_extent.width;
    const uint64_t        extent_top    = static_cast<uint64_t>(in_extent.y) + in_extent.height;
    bool                  result        = false;
    std::vector<uint64_t> x_vec;

    if (in_extent.is_known &&
        in_extent.width    != 0 &&
        in_extent.height   != 0)
    {
        for (const auto& rectangle : in_rectangle_vec)
        {
            if (rectangle.is_known &&
                rectangle.width    != 0 &&
                rectangle.height   != 0)
            {
                Edges edges;

                edges.bottom = std::max<uint64_t>(extent_bottom,
                                                  rectangle.y);
                edges.left   = std::max<uint64_t>(extent_left,
                                                  rectangle.x);
                edges.right  = std::min<uint64_t>(extent_right,
                                                  static_cast<uint64_t>(rectangle.x) + rectangle.width);
                edges.top    = std::min<uint64_t>(extent_top,
                                                  static_cast<uint64_t>(rectangle.y) + rectangle.height);

                if (edges.left   <  edges.right &&
                    edges.bottom <  edges.top)
                {
                    edges_vec.push_back(edges);

                    x_vec.push_back(edges.left);
                    x_vec.push_back(edges.right);
                }
            }
        }

        x_vec.push_back(extent_left);
        x_vec.push_back(extent_right);

        std::sort(x_vec.begin(),
                  x_vec.end  () );

        x_vec.erase(std::unique(x_vec.begin(),
                                x_vec.end  () ),
                    x_vec.end() );

        /* Each slab between two edges is covered if the rectangles that span it cover the extent from the bottom to the top; and the extent is covered if
         * every slab of it is.
         */
        result = (x_vec.size() >= 2);

        for (size_t n_slab = 0;
                    n_slab + 1 < x_vec.size() && result;
                  ++n_slab)
        {
            uint64_t                                    covered_up_to = extent_bottom;
            std::vector<std::pair<uint64_t, uint64_t> > interval_vec;
            const uint64_t                              slab_left     = x_vec.at(n_slab);
            const uint64_t                              slab_right    = x_vec.at(n_slab + 1);

            for (const auto& edges : edges_vec)
            {
                if (edges.left  <= slab_left &&
                    edges.right >= slab_right)
                {
                    interval_vec.push_back(std::make_pair(edges.bottom,
                                                          edges.top) );
                }
            }

            std::sort(interval_vec.begin(),
                      interval_vec.end  () );

            for (const auto& interval : interval_vec)
            {
                if (interval.first > covered_up_to)
                {
                    break;
                }

                covered_up_to = std::max<uint64_t>(covered_up_to,
                                                   interval.second);
            }

            result = (covered_up_to >= extent_top);
        }
    }

    return result;
}
