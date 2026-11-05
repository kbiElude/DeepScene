/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_REPLAY_ENGINEDLL_D3D7_DRAW_REWRITER_H
#define STEREO_REPLAY_ENGINEDLL_D3D7_DRAW_REWRITER_H

#include "Capture/APIDumpLoader/APIDumpLoader.h"
#include "Stereo/D3D7/screen_parallax.h"
#include "overlay_chunk_source.h"
#include <functional>
#include <stddef.h>
#include <stdint.h>

namespace Stereo
{
    namespace Replay
    {
        namespace EngineDLL
        {
            /* Gives the draws of a dumped Direct3D 7 call parallax for an eye, when the vertices of the draw are already in the space of the
             * picture: the call is copied with its vertices moved (see Stereo::D3D7::ScreenParallax for how, and why), in a chunk of the
             * overlay.
             */
            class D3D7DrawRewriter
            {
            public:
                /* Returns false if the call is not a draw of pre-transformed vertices which this changes (a draw of another kind, vertices which a matrix
                 * moves, a vertex buffer, or data which does not fit what the call says). Otherwise the call is copied to the output with the vertices
                 * pointing to a chunk of the overlay, which holds them moved.
                 */
                static bool rewrite(const Capture::APIDumpLoader::DumpedAPICall&    in_call,
                                    const ::Replay::ReplayEngineDLL::IChunkSource&  in_chunk_source,
                                    const Stereo::D3D7::ScreenParallax::Parameters& in_parameters,
                                    OverlayChunkSource*                             inout_overlay_ptr,
                                    Capture::APIDumpLoader::DumpedAPICall*          out_call_ptr);

                /* The same, with every vertex moved by the same shift, in pixels (positive to the right): for a draw which is pushed toward the viewer as
                 * a whole, whatever its depth (an interface). Returns false for a shift of 0, too.
                 */
                static bool rewrite_by(const Capture::APIDumpLoader::DumpedAPICall&   in_call,
                                       const ::Replay::ReplayEngineDLL::IChunkSource& in_chunk_source,
                                       const float&                                   in_shift,
                                       OverlayChunkSource*                            inout_overlay_ptr,
                                       Capture::APIDumpLoader::DumpedAPICall*         out_call_ptr);

                /* Whether the vertices which the call uses reach both sides of a picture of the width (see ScreenParallax::reaches_both_sides). Returns false if
                 * the call is not a draw of pre-transformed vertices with its data in the workload: nothing is known of where they are then.
                 */
                static bool find_extent(const Capture::APIDumpLoader::DumpedAPICall&   in_call,
                                        const ::Replay::ReplayEngineDLL::IChunkSource& in_chunk_source,
                                        const float&                                   in_picture_width,
                                        bool*                                          out_reaches_both_sides_ptr);

            private:
                /* What is done to the vertices of a draw: their size in bytes, how many of them there are, and where they are. */
                typedef std::function<void(const uint32_t& in_vertex_n_bytes,
                                           const size_t&   in_n_vertices,
                                           uint8_t*        inout_vertex_ptr)> ShiftFunc;

                /* What rewrite() and rewrite_by() are, but for how the vertices are moved. */
                static bool rewrite_vertices(const Capture::APIDumpLoader::DumpedAPICall&   in_call,
                                             const ::Replay::ReplayEngineDLL::IChunkSource& in_chunk_source,
                                             const ShiftFunc&                               in_shift_func,
                                             OverlayChunkSource*                            inout_overlay_ptr,
                                             Capture::APIDumpLoader::DumpedAPICall*         out_call_ptr);
            };
        }
    }
}

#endif /* STEREO_REPLAY_ENGINEDLL_D3D7_DRAW_REWRITER_H */
