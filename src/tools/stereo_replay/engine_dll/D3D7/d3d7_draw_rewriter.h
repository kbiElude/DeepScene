/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_REPLAY_ENGINEDLL_D3D7_DRAW_REWRITER_H
#define STEREO_REPLAY_ENGINEDLL_D3D7_DRAW_REWRITER_H

#include "capture/api_dump_loader/api_dump_loader.h"
#include "stereo/core/screen_parallax.h"
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
            /* Applies parallax for an eye to a dumped Direct3D 7 draw whose vertices are in pixels. The call is copied with the vertices moved (see
             * Stereo::Core::ScreenParallax).
             */
            class D3D7DrawRewriter
            {
            public:
                /* Returns false if the call is not a draw of pre-transformed vertices that this class can change: a draw of another kind, vertices that a
                 * matrix moves, a vertex buffer, or data that does not match what the call says. Otherwise the call is copied to the output, with its
                 * vertices pointing to a chunk of the overlay that holds the moved vertices.
                 */
                static bool rewrite(const Capture::APIDumpLoader::DumpedAPICall&    in_call,
                                    const ::Replay::ReplayEngineDLL::IChunkSource&  in_chunk_source,
                                    const Stereo::Core::ScreenParallax::Parameters& in_parameters,
                                    OverlayChunkSource*                             inout_overlay_ptr,
                                    Capture::APIDumpLoader::DumpedAPICall*          out_call_ptr);

                /* The same, but every vertex is moved by the same shift, in pixels (positive to the right). It is for a draw that is pushed toward the
                 * viewer as a whole, whatever its depth (an interface). Also returns false for a shift of 0.
                 */
                static bool rewrite_by(const Capture::APIDumpLoader::DumpedAPICall&   in_call,
                                       const ::Replay::ReplayEngineDLL::IChunkSource& in_chunk_source,
                                       const float&                                   in_shift,
                                       OverlayChunkSource*                            inout_overlay_ptr,
                                       Capture::APIDumpLoader::DumpedAPICall*         out_call_ptr);

                /* Says whether the vertices that the call uses reach both sides of a picture of the given width (see ScreenParallax::reaches_both_sides).
                 * Returns false if the call is not a draw of pre-transformed vertices with its data in the workload, because the vertex positions are then
                 * unknown.
                 */
                static bool find_extent(const Capture::APIDumpLoader::DumpedAPICall&   in_call,
                                        const ::Replay::ReplayEngineDLL::IChunkSource& in_chunk_source,
                                        const float&                                   in_picture_width,
                                        bool*                                          out_reaches_both_sides_ptr);

            private:
                /* Moves the vertices of a draw. It gets the size of one vertex in bytes, the number of vertices and a pointer to the vertex data. */
                typedef std::function<void(const uint32_t& in_vertex_n_bytes,
                                           const size_t&   in_n_vertices,
                                           uint8_t*        inout_vertex_ptr)> ShiftFunc;

                /* The common implementation of rewrite() and rewrite_by(); the shift function decides how the vertices are moved. */
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
