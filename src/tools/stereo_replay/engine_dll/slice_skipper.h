/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_REPLAY_ENGINEDLL_SLICE_SKIPPER_H
#define STEREO_REPLAY_ENGINEDLL_SLICE_SKIPPER_H

#include "analysis/api_dump_dependencies/types.h"
#include "gl_display_list_tracker.h"
#include "replay/replay_engine_dll/i_call_interceptor.h"
#include <atomic>
#include <deque>
#include <memory>
#include <mutex>

namespace Stereo
{
    namespace Replay
    {
        namespace EngineDLL
        {
            /* Leaves out what the dead frames of a slice make (see FrameSlicePlanner): the draws, the clears, the copies and the uploads. The calls
             * which set the state of the device or make and release objects are made, since the frames which are not dead go on from the state which
             * the dead ones leave, and the objects they use must be there. Of OpenGL, the calls that only give a piece of state a value that the groups of a
             * glPushAttrib save are left out between it and its glPopAttrib as well, in the reader (see GlAttributeCoverage), since the pop undoes them.
             */
            class SliceSkipper : public ::Replay::ReplayEngineDLL::ICallInterceptor
            {
            public:
                SliceSkipper();

                /* No frame is left out. Any thread. */
                void clear_slice();

                /* Tells the reader of the workload which calls of the dead frames of the slice are not made into calls at all (the content calls of OpenGL: its
                 * workloads are mostly vertices, which a replay that leaves them out should not have to build); empty if there is no slice. The calls of a
                 * display list that is being made are kept, and a list that is made across two segments ends the process (AI_ASSERT_ALWAYS), as what is left
                 * out of its first part is left out of the list. It holds the slice as it is when asked, so it is asked again after a new slice.
                 */
                Capture::APIDumpLoader::CallKeptFuncFactory get_call_kept_func_factory() const;

                /* Tells the reader of the workload which data chunks the slice leaves unread (those of the content calls of its dead frames); empty if there is no
                 * slice. It holds the slice as it is when asked, so it is asked again after a new slice.
                 */
                Capture::APIDumpLoader::ChunkWantedFunc get_chunk_wanted_func() const;

                /* Whether the call makes content (what the dead frames of a slice leave out); false for the calls that set state, make or release an object. */
                static bool is_content_call(const APIInterceptor::APIFunction& in_function);

                void on_call_replayed(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                      const uint32_t&                              in_n_frame,
                                      const bool&                                  in_was_executed) override;

                /* Whether the slice leaves out the content of the frame; false if there is no slice. Any thread. */
                bool is_dead_frame(const uint32_t& in_n_frame) const;

                /* The frames which the slice says are dead are left out from now on. Any thread. */
                void set_slice(const Analysis::APIDumpDependencies::FrameSlice& in_slice);

                bool should_execute_call(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                         const uint32_t&                              in_n_frame) override;

            private:
                /* Whether a call of a display list in a dead frame is left out: it is if the list only draws (what it does that is not content is state that the frames
                 * after it go on from, so a list that has any is made all the same). A list that is related to the location of the eye ends the process
                 * (AI_ASSERT_ALWAYS), as what it does to the matrices and the viewport is not done for a call that is left out.
                 */
                bool is_list_call_skippable(const Capture::APIDumpLoader::DumpedAPICall& in_call) const;

                /* The replay thread looks at the slice at every call, so it is not locked for that: a slice which is replaced is kept, since the replay may be
                 * looking at it still. A slice is a bit for each frame of the workload, and a person seeks a few times.
                 */
                GlDisplayListTracker                                                    m_list_tracker; /* the replay thread only */
                std::mutex                                                              m_slice_mutex;
                std::atomic<const Analysis::APIDumpDependencies::FrameSlice*>           m_slice_ptr;
                std::deque<std::unique_ptr<Analysis::APIDumpDependencies::FrameSlice> > m_slice_ptr_deque;
            };
        }
    }
}

#endif /* STEREO_REPLAY_ENGINEDLL_SLICE_SKIPPER_H */
