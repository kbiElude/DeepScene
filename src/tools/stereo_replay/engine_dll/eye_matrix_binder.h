/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_REPLAY_ENGINEDLL_EYE_MATRIX_BINDER_H
#define STEREO_REPLAY_ENGINEDLL_EYE_MATRIX_BINDER_H

#include "frame_buffer.h"
#include "frame_rewriter.h"
#include "overlay_chunk_source.h"
#include "types.h"
#include <functional>
#include <map>
#include <memory>
#include <utility>
#include <vector>

namespace Stereo
{
    namespace Replay
    {
        namespace EngineDLL
        {
            /* Decides which matrix each draw is made with. A call that sets a matrix the recipe may change is not executed when it is
             * read. The binder keeps it and executes it just before the draw that needs it: as the application made it for a draw the
             * recipe does not affect, or rewritten for the eye for an affected draw. A kept call is executed again only when the next
             * draw needs the other variant. This works with any API, as long as a call sets the matrix.
             *
             * Kept calls last from frame to frame, because an application often sets a matrix once and draws with it for many frames.
             */
            class EyeMatrixBinder
            {
            public:
                typedef std::function<bool(const Capture::APIDumpLoader::DumpedAPICall&   in_call,
                                           const ::Replay::ReplayEngineDLL::IChunkSource& in_chunk_source)> ExecuteFunc;

                EyeMatrixBinder(const FrameRewriter& in_rewriter,
                                const ExecuteFunc&   in_execute_func);

                /* Executes the matrix calls the draw needs: the rewritten ones for the eye if the draw is affected, otherwise the
                 * original ones. Returns false if executing a call fails.
                 */
                bool before_draw(const bool&                  in_is_affected,
                                 const Analysis::Recipe::Eye& in_eye,
                                 const float&                 in_eye_separation);

                /* Drops the kept calls. Use it after other calls have changed the matrices in the device. */
                void forget();

                /* Offers a call of the frame to the binder. Returns true if the binder keeps it to execute later. Otherwise the caller
                 * executes it, and any call kept for the same matrix is dropped because this call replaces it.
                 */
                bool on_call(const Capture::APIDumpLoader::DumpedAPICall&   in_call,
                             const ::Replay::ReplayEngineDLL::IChunkSource& in_chunk_source,
                             const Analysis::Recipe::CallPlace&             in_place,
                             const Analysis::Recipe::Eye&                   in_eye,
                             const float&                                   in_eye_separation);

            private:
                /* Identifies a matrix by the function and the integer arguments of its call (for example the state D3DTS_VIEW). */
                typedef std::pair<APIInterceptor::APIFunction, std::vector<uint32_t> > SlotKey;

                /* A call that sets a matrix, with everything needed to execute it later. A slot must not be copied or moved because
                 * overlay refers to its buffer member.
                 */
                struct Slot
                {
                    Slot();

                    EyeMatrixBinding                      bound;
                    float                                 bound_separation;  /* the separation the matrix in the device was made for */
                    FrameBuffer                           buffer;            /* the call as the application made it, with its chunks */
                    std::unique_ptr<OverlayChunkSource>   overlay;           /* the buffer overlaid with the rewritten call's matrix */
                    Analysis::Recipe::CallPlace           place;
                    Capture::APIDumpLoader::DumpedAPICall rewritten_call;
                    Analysis::Recipe::Eye                 rewritten_eye;
                    float                                 rewritten_separation;
                    bool                                  has_rewritten_call;
                };

                /* Rewrites the call of the slot for the eye. Returns false if the recipe does not change the call. */
                bool rewrite(const Analysis::Recipe::Eye& in_eye,
                             const float&                 in_eye_separation,
                             Slot*                        inout_slot_ptr) const;

                static SlotKey get_slot_key(const Capture::APIDumpLoader::DumpedAPICall& in_call);

                ExecuteFunc                               m_execute_func;
                const FrameRewriter&                      m_rewriter;
                std::map<SlotKey, std::unique_ptr<Slot> > m_slot_map;
            };
        }
    }
}

#endif /* STEREO_REPLAY_ENGINEDLL_EYE_MATRIX_BINDER_H */
