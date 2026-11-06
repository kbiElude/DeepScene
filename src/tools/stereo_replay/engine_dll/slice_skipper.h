/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_REPLAY_ENGINEDLL_SLICE_SKIPPER_H
#define STEREO_REPLAY_ENGINEDLL_SLICE_SKIPPER_H

#include "analysis/api_dump_dependencies/types.h"
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
             * the dead ones leave, and the objects they use must be there.
             */
            class SliceSkipper : public ::Replay::ReplayEngineDLL::ICallInterceptor
            {
            public:
                SliceSkipper();

                /* No frame is left out. Any thread. */
                void clear_slice();

                /* Tells the reader of the workload which data chunks the slice leaves unread (those of the content calls of its dead frames); empty if there is no
                 * slice. It holds the slice as it is now, so it is asked again after a new slice.
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
                /* The replay thread looks at the slice at every call, so it is not locked for that: a slice which is replaced is kept, since the replay may be
                 * looking at it still. A slice is a bit for each frame of the workload, and a person seeks a few times.
                 */
                std::atomic<const Analysis::APIDumpDependencies::FrameSlice*>           m_slice_ptr;
                std::mutex                                                              m_slice_mutex;
                std::deque<std::unique_ptr<Analysis::APIDumpDependencies::FrameSlice> > m_slice_ptr_deque;
            };
        }
    }
}

#endif /* STEREO_REPLAY_ENGINEDLL_SLICE_SKIPPER_H */
