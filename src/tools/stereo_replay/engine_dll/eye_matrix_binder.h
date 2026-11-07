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
            /* Decides which matrix each draw is made with. The calls which set a matrix a recipe changes are not executed where they
             * are: they are kept, and executed before the draw which needs them, as the application made them for a draw which the recipe
             * leaves alone, and as the recipe changes them for the eye for the others. A call is executed again only when the next draw
             * needs the other one. That is the same whatever the API does with matrices, as long as a call sets them.
             *
             * What is kept lasts from frame to frame, since an application sets a matrix once and draws with it for many frames.
             */
            class EyeMatrixBinder
            {
            public:
                typedef std::function<bool(const Capture::APIDumpLoader::DumpedAPICall&   in_call,
                                           const ::Replay::ReplayEngineDLL::IChunkSource& in_chunk_source)> ExecuteFunc;

                EyeMatrixBinder(const FrameRewriter& in_rewriter,
                                const ExecuteFunc&   in_execute_func);

                /* Executes what the draw needs: for a draw which is affected, the matrices for the eye, for another one the matrices as
                 * the application set them. Returns false if executing a call fails.
                 */
                bool before_draw(const bool&                  in_is_affected,
                                 const Analysis::Recipe::Eye& in_eye,
                                 const float&                 in_eye_separation);

                /* Forgets the calls kept, for when other calls have changed what the device has. */
                void forget();

                /* A call of the frame. Returns true if it is taken, and so executed later; otherwise it is the caller's to execute, and a
                 * call kept before for the same matrix is dropped, since this one replaces it.
                 */
                bool on_call(const Capture::APIDumpLoader::DumpedAPICall&   in_call,
                             const ::Replay::ReplayEngineDLL::IChunkSource& in_chunk_source,
                             const Analysis::Recipe::CallPlace&             in_place,
                             const Analysis::Recipe::Eye&                   in_eye,
                             const float&                                   in_eye_separation);

            private:
                /* A matrix is told by the function and the integer arguments of its call (the state of D3DTS_VIEW, say). */
                typedef std::pair<APIInterceptor::APIFunction, std::vector<uint32_t> > SlotKey;

                /* A call that sets a matrix, with everything needed to execute it later. It does not move: it refers to its own members. */
                struct Slot
                {
                    Slot();

                    EyeMatrixBinding                      bound;
                    float                                 bound_separation;  /* the separation the matrix in the device was made for */
                    FrameBuffer                           buffer;            /* the call as the application made it, with its chunks */
                    std::unique_ptr<OverlayChunkSource>   overlay;           /* the buffer, with the matrix of the rewritten call  */
                    Analysis::Recipe::CallPlace           place;
                    Capture::APIDumpLoader::DumpedAPICall rewritten_call;
                    Analysis::Recipe::Eye                 rewritten_eye;
                    float                                 rewritten_separation;
                    bool                                  has_rewritten_call;
                };

                /* Makes the call of the slot for the eye. Returns false if the recipe changes nothing. */
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
