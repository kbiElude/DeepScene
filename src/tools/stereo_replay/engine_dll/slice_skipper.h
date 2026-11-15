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
            /* Leaves out what the dead frames of a slice produce (see FrameSlicePlanner): draws, clears, copies and uploads. Calls that
             * set the device state or that create and release objects are still executed, because the live frames continue from the
             * state the dead frames leave and use the objects they create. For OpenGL, the reader also drops the calls between a
             * glPushAttrib and its glPopAttrib that only assign a value to state saved by the pushed groups (see GLAttributeCoverage),
             * because the pop undoes them.
             */
            class SliceSkipper : public ::Replay::ReplayEngineDLL::ICallInterceptor
            {
            public:
                SliceSkipper();

                /* Removes the slice, so no frame is left out. Any thread. */
                void clear_slice();

                /* Creates the function that tells the workload reader which calls of the dead frames of the slice are not turned into
                 * calls at all. These are the content calls of OpenGL: its workloads are mostly vertices, which a replay that leaves them
                 * out should not have to build. Returns an empty factory if there is no slice. Calls of a display list that is being
                 * recorded are kept. A list recorded across two segments ends the process (AI_ASSERT_ALWAYS), because what is left out
                 * of its first part is left out of the list. The factory captures the slice as it is when asked, so ask again after a
                 * new slice.
                 */
                Capture::APIDumpLoader::CallKeptFuncFactory get_call_kept_func_factory() const;

                /* Creates the function that tells the workload reader which data chunks the slice leaves unread: those of the content
                 * calls of its dead frames. Returns an empty function if there is no slice. The function captures the slice as it is when
                 * asked, so ask again after a new slice.
                 */
                Capture::APIDumpLoader::ChunkWantedFunc get_chunk_wanted_func() const;

                /* Whether the call produces content, which the dead frames of a slice leave out. False for calls that set state or
                 * create or release an object.
                 */
                static bool is_content_call(const APIInterceptor::APIFunction& in_function);

                void on_call_replayed(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                      const uint32_t&                              in_n_frame,
                                      const bool&                                  in_was_executed) override;

                /* Whether the slice leaves out the content of the frame; false if there is no slice. Any thread. */
                bool is_dead_frame(const uint32_t& in_n_frame) const;

                /* From now on, leaves out the frames that the slice marks as dead. Any thread. */
                void set_slice(const Analysis::APIDumpDependencies::FrameSlice& in_slice);

                bool should_execute_call(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                         const uint32_t&                              in_n_frame) override;

            private:
                /* Whether a call to a display list in a dead frame is left out. It is left out if the list only draws. Anything else the
                 * list does is state that later frames continue from, so a list that does any of it is executed. A list that depends on
                 * the location of the eye ends the process (AI_ASSERT_ALWAYS), because what it does to the matrices and the viewport is
                 * not done when its call is left out.
                 */
                bool is_list_call_skippable(const Capture::APIDumpLoader::DumpedAPICall& in_call) const;

                /* The replay thread reads the slice at every call, so reading it takes no lock. A replaced slice is kept, because the
                 * replay thread may still be reading it. This is cheap: a slice is one bit per frame of the workload, and a person
                 * seeks only a few times.
                 */
                GLDisplayListTracker                                                    m_list_tracker; /* the replay thread only */
                std::mutex                                                              m_slice_mutex;
                std::atomic<const Analysis::APIDumpDependencies::FrameSlice*>           m_slice_ptr;
                std::deque<std::unique_ptr<Analysis::APIDumpDependencies::FrameSlice> > m_slice_ptr_deque;
            };
        }
    }
}

#endif /* STEREO_REPLAY_ENGINEDLL_SLICE_SKIPPER_H */
