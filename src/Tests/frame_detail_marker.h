/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Makes the detail of a frame that carries a number, so that a test can tell which frame's detail it is looking at.
 */
#ifndef TESTS_FRAME_DETAIL_MARKER_H
#define TESTS_FRAME_DETAIL_MARKER_H

#include "Analysis/APIDumpAnalysis/types.h"
#include <stdint.h>

namespace Tests
{
    class FrameDetailMarker
    {
    public:
        static Analysis::APIDumpAnalysis::FrameDetail make(const uint32_t&                               in_marker);
        static uint32_t                               read(const Analysis::APIDumpAnalysis::FrameDetail& in_detail);
    };
}

#endif /* TESTS_FRAME_DETAIL_MARKER_H */
