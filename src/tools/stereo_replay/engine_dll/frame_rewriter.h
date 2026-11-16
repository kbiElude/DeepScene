/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_REPLAY_ENGINEDLL_FRAME_REWRITER_H
#define STEREO_REPLAY_ENGINEDLL_FRAME_REWRITER_H

#include "analysis/recipe/recipe_applier.h"
#include "overlay_chunk_source.h"

namespace Stereo
{
    namespace Replay
    {
        namespace EngineDLL
        {
            /* Rewrites the matrix argument of the calls of a frame for one eye, as the rules of a recipe say. */
            class FrameRewriter
            {
            public:
                explicit FrameRewriter(const Analysis::Recipe::RecipeApplier& in_applier);

                /* Whether any rule applies to the call. This is a cheap check that does not read the matrix. A call for which it returns
                 * false never needs to be kept for rewriting.
                 */
                bool has_matching_rule(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                       const Analysis::Recipe::CallPlace&           in_place) const;

                /* Returns false if no rule changes the call. Otherwise it writes a copy of the call to the output. In the copy the
                 * matrix argument refers to a new chunk in the overlay, and that chunk holds the changed matrix. The convergence distance is
                 * where the eyes converge, for the rules that converge them (see RecipeApplier::apply_rule()); 0 means nowhere.
                 */
                bool rewrite(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                             const Analysis::Recipe::CallPlace&           in_place,
                             const Analysis::Recipe::Eye&                 in_eye,
                             const float&                                 in_eye_separation,
                             OverlayChunkSource*                          inout_overlay_ptr,
                             Capture::APIDumpLoader::DumpedAPICall*       out_call_ptr,
                             const float&                                 in_convergence_distance = 0.0f) const;

            private:
                const Analysis::Recipe::RecipeApplier& m_applier;
            };
        }
    }
}

#endif /* STEREO_REPLAY_ENGINEDLL_FRAME_REWRITER_H */
