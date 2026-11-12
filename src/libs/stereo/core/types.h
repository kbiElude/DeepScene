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
        /* How the distances of a depth buffer are counted (see DepthMeasurer): the first bin that is for a distance (the ones before it are for what is not one, a bin for each of the 8 pixels of a step of the pass so that they do not wait for each other), the bits of a distance that are left out to make the number of its bin, how many bins a histogram has,
         * and how many histograms there are (a power of 2).
         */
        struct DepthHistogramLayout
        {
            uint32_t first_distance_key = 0;
            uint32_t key_shift        = 0;
            uint32_t n_bins           = 0;
            uint32_t n_sub_histograms = 0;
        };

        /* The bins of the histogram of the distances in a depth buffer that were used: the lowest and the highest (the lowest is above the highest if none was). */
        struct DepthKeyRange
        {
            uint32_t max_key = 0;
            uint32_t min_key = UINT32_MAX;
        };

        /* What the pass over the pixels of a depth buffer found out (see DepthMeasurer): the bins of the histogram that were used, and how many of the pixels were not drawn to. */
        struct DepthPassResult
        {
            DepthKeyRange key_range;
            size_t        n_sky = 0;
        };

        /* What is looked for in the distances of the pixels that were drawn to (see DepthBufferStatistics::get_selection). */
        struct DepthSelection
        {
            float  far_plane = 0.0f; /* how far the sky is */
            bool   has_sky   = false;
            bool   is_enough = false; /* false if there is too little drawn for the buffer to say how deep the scene is */
            size_t n_edge    = 0;     /* how many of the nearest (and of the farthest) pixels are the edge of the scene */
        };

        /* Called when the converter finds that the application is one it cannot convert right. It is not expected to return: what it
         * does instead (log and end the process, say) is the caller's to decide.
         */
        typedef std::function<void(const std::string& in_message)> FailFunc;

        /* Called to say what the converter did, for the log of the session. */
        typedef std::function<void(const std::string& in_message)> LogFunc;

        /* A matrix to put in the device before a draw. The function and the arguments are those of the call with which the application set
         * it, whose matrix argument is the one to replace; the caller makes the call again with this matrix.
         */
        struct MatrixChange
        {
            std::vector<APIInterceptor::APIFunctionArgument> argument_vec;
            APIInterceptor::APIFunction                      function          = APIInterceptor::APIFunction::APIFUNCTION_UNKNOWN;
            std::array<float, 16>                            matrix_fp32vec16  = {};
        };

        /* What the device has of a matrix, as far as the slots know. */
        enum class MatrixSlotBinding
        {
            LEFT,
            MONO,
            NOTHING, /* the matrix was set by the application, and has not been put in the device since */
            OVERLAY_LEFT,
            OVERLAY_RIGHT,
            RIGHT
        };

        /* A read of the depth buffer that was started, and whose result is not taken yet (see FrameDepthLedger): the frame that was being made, the place that the adapter has
         * the result in, and the numbers of the projection of the pass that the result is of, which are needed to make sense of it.
         */
        struct PendingDepthRead
        {
            uint64_t frame   = 0;
            uint32_t slot    = 0;
            float    x_scale = 0.0f;
            float    z_scale = 0.0f;
            float    z_shift = 0.0f;
        };

        /* How the x of the vertices are moved. */
        /* A rectangle of pixels: where its top left corner is, and how large it is. */
        struct PixelRect
        {
            uint32_t height = 0;
            uint32_t width  = 0;
            uint32_t x      = 0;
            uint32_t y      = 0;
        };

        enum class ScreenShiftKind
        {
            NONE,       /* as they are                                                              */
            PER_VERTEX, /* each by its depth, with the distances below (the draw is part of the scene) */
            UNIFORM     /* all by the same pixels (the draw is pushed toward the viewer as a whole)    */
        };
    }
}

#endif /* STEREO_CORE_TYPES_H */
