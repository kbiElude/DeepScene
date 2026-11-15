/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_CORE_TYPES_H
#define STEREO_CORE_TYPES_H

#include "common/api_function_argument.h"
#include "common/types.h"
#include <array>
#include <functional>
#include <stdint.h>
#include <string>
#include <vector>

namespace Stereo
{
    namespace Core
    {
        /* How DepthMeasurer counts the distances of a depth buffer. first_distance_key is the first bin used for a distance. The bins before it
         * are for pixels without a distance, one bin for each of the 8 pixels that the pixel pass handles together, so that these pixels never
         * wait for each other. key_shift is the number of low bits of a distance that are dropped to get its bin. n_bins is the number of bins
         * in one histogram. n_sub_histograms is the number of histograms (a power of 2).
         */
        struct DepthHistogramLayout
        {
            uint32_t first_distance_key = 0;
            uint32_t key_shift        = 0;
            uint32_t n_bins           = 0;
            uint32_t n_sub_histograms = 0;
        };

        /* The range of histogram bins that were used for the distances of a depth buffer: the lowest and the highest. The lowest is above the
         * highest if no bin was used.
         */
        struct DepthKeyRange
        {
            uint32_t max_key = 0;
            uint32_t min_key = UINT32_MAX;
        };

        /* The result of the pass over the pixels of a depth buffer (see DepthMeasurer): the range of histogram bins that were used and the
         * number of pixels that were not drawn to.
         */
        struct DepthPassResult
        {
            DepthKeyRange key_range;
            size_t        n_sky = 0;
        };

        /* What to look for among the distances of the pixels that were drawn to (see DepthBufferStatistics::get_selection). */
        struct DepthSelection
        {
            float  far_plane = 0.0f; /* the distance of the sky (the far plane) */
            bool   has_sky   = false;
            bool   is_enough = false; /* false if too little was drawn for the buffer to say how deep the scene is */
            size_t n_edge    = 0;     /* how many of the nearest (and of the farthest) pixels form the edge of the scene */
        };

        /* Called when the converter finds that it cannot convert the application correctly. It is not expected to return. What it does
         * instead (for example, log and end the process) is up to whoever supplies it.
         */
        typedef std::function<void(const std::string& in_message)> FailFunc;

        /* Called to report what the converter did, for the session log. */
        typedef std::function<void(const std::string& in_message)> LogFunc;

        /* A matrix to put in the device before a draw. function and argument_vec are those of the application's call that set the matrix.
         * The caller repeats that call with matrix_fp32vec16 in place of the call's matrix argument.
         */
        struct MatrixChange
        {
            std::vector<APIInterceptor::APIFunctionArgument> argument_vec;
            APIInterceptor::APIFunction                      function          = APIInterceptor::APIFunction::APIFUNCTION_UNKNOWN;
            std::array<float, 16>                            matrix_fp32vec16  = {};
        };

        /* Which version of a matrix the device holds, as far as MatrixSlots knows. */
        enum class MatrixSlotBinding
        {
            LEFT,
            MONO,
            NOTHING, /* the application set the matrix, and nothing has been put in the device since */
            OVERLAY_LEFT,
            OVERLAY_RIGHT,
            RIGHT
        };

        /* A depth buffer read that was started and whose result has not been taken yet (see FrameDepthLedger). It records the frame that was
         * being made, the slot where the adapter holds the result, and the projection values of the pass the result belongs to. The
         * projection values are needed to interpret the result.
         */
        struct PendingDepthRead
        {
            uint64_t frame   = 0;
            uint32_t slot    = 0;
            float    x_scale = 0.0f;
            float    z_scale = 0.0f;
            float    z_shift = 0.0f;
        };

        /* A rectangle of pixels: the position of its top left corner and its size. */
        struct PixelRect
        {
            uint32_t height = 0;
            uint32_t width  = 0;
            uint32_t x      = 0;
            uint32_t y      = 0;
        };

        /* How the x coordinates of the vertices of a draw are moved. */
        enum class ScreenShiftKind
        {
            NONE,       /* vertices stay as they are                                                                 */
            PER_VERTEX, /* each vertex by its depth, using the distances in the plan (the draw is part of the scene) */
            UNIFORM     /* all by the same number of pixels (the draw is pushed toward the viewer as a whole)        */
        };
    }
}

#endif /* STEREO_CORE_TYPES_H */
