/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREOREPLAYENGINEDLL_FRAME_REWRITER_H
#define STEREOREPLAYENGINEDLL_FRAME_REWRITER_H

#include "Analysis/RecipeApplier/recipe_applier.h"
#include "overlay_chunk_source.h"

namespace Replay
{
    namespace StereoReplayEngineDLL
    {
        /* Changes the matrices of the calls of a frame for an eye, as the rules of a recipe say. */
        class FrameRewriter
        {
        public:
            explicit FrameRewriter(const Analysis::RecipeApplier::RecipeApplier& in_applier);

            /* Whether a rule is about the call: a quick check which does not look at the matrix, so a call for which it is false is
             * not worth keeping.
             */
            bool has_matching_rule(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                   const Analysis::RecipeApplier::CallPlace&    in_place) const;

            /* Returns false if no rule changes the call. Otherwise the call is copied to the output with the matrix argument
             * pointing to a chunk of the overlay, which holds the new matrix.
             */
            bool rewrite(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                         const Analysis::RecipeApplier::CallPlace&    in_place,
                         const Analysis::RecipeLoader::Eye&           in_eye,
                         const float&                                 in_eye_separation,
                         OverlayChunkSource*                          inout_overlay_ptr,
                         Capture::APIDumpLoader::DumpedAPICall*       out_call_ptr) const;

        private:
            const Analysis::RecipeApplier::RecipeApplier& m_applier;
        };
    }
}

#endif /* STEREOREPLAYENGINEDLL_FRAME_REWRITER_H */
