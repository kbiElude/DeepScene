/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_REPLAY_ENGINEDLL_SLICE_SKIPPER_H
#define STEREO_REPLAY_ENGINEDLL_SLICE_SKIPPER_H

#include "analysis/api_dump_dependencies/types.h"
#include "i_slice_call_handler.h"
#include "replay/replay_engine_dll/i_call_interceptor.h"
#include <array>
#include <atomic>
#include <deque>
#include <memory>
#include <mutex>
#include <vector>

namespace Stereo
{
    namespace Replay
    {
        namespace EngineDLL
        {
            /* Leaves out what the dead frames of a slice produce (see FrameSlicePlanner): draws, clears, copies and uploads. Calls that set
             * the device state or that create and release objects are still executed, because the live frames continue from the state the dead
             * frames leave and use the objects they create. The slice is kept here; what a dead frame does to the calls of an API is up to the
             * handler of that API (see ISliceCallHandler), and it is given the calls of that API only.
             */
            class SliceSkipper : public ::Replay::ReplayEngineDLL::ICallInterceptor
            {
            public:
                SliceSkipper();

                /* Removes the slice, so no frame is left out. Any thread. */
                void clear_slice();

                /* The APIs that have a handler. */
                std::vector<Capture::APIDumpLoader::APIGroup> get_api_groups() const override;

                /* Creates the function that tells the workload reader which calls of the dead frames of the slice are not turned into calls at
                 * all (see ISliceCallHandler::create_call_kept_func). Returns an empty factory if there is no slice. The factory captures the
                 * slice as it is when asked, so ask again after a new slice.
                 */
                Capture::APIDumpLoader::CallKeptFuncFactory get_call_kept_func_factory() const;

                /* Creates the function that tells the workload reader which data chunks the slice leaves unread: those of the calls of its dead
                 * frames that the handlers do not want. Returns an empty function if there is no slice. The function captures the slice as it is
                 * when asked, so ask again after a new slice.
                 */
                Capture::APIDumpLoader::ChunkWantedFunc get_chunk_wanted_func() const;

                /* Wants the segments whose frames are all dead as state calls only: they come from the state frames of the index. Empty if there
                 * is no slice. The calls a state frame keeps are those that should_execute_call() does not leave out of a dead frame, whatever
                 * the slice.
                 */
                Capture::APIDumpLoader::StateOnlyFunc get_state_only_func() const;

                /* Whether the function produces content, or may (a call of a display list), which the dead frames of a slice leave out. False for
                 * calls that set state or create or release an object.
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
                /* The handler of the API, or nullptr if it has none (its calls are all executed). */
                ISliceCallHandler* find_handler_ptr(const Capture::APIDumpLoader::APIGroup& in_group) const;

                /* The replay thread reads the slice at every call, so reading it takes no lock. A replaced slice is kept, because the
                 * replay thread may still be reading it. This is cheap: a slice is one bit per frame of the workload, and a person
                 * seeks only a few times.
                 */
                std::array<ISliceCallHandler*, static_cast<size_t>(Capture::APIDumpLoader::APIGroup::UNKNOWN) + 1> m_handler_by_group_ptr_arr; /* nullptr: no handler */
                std::vector<std::unique_ptr<ISliceCallHandler> >                        m_handler_ptr_vec;
                std::mutex                                                              m_slice_mutex;
                std::atomic<const Analysis::APIDumpDependencies::FrameSlice*>           m_slice_ptr;
                std::deque<std::unique_ptr<Analysis::APIDumpDependencies::FrameSlice> > m_slice_ptr_deque;
            };
        }
    }
}

#endif /* STEREO_REPLAY_ENGINEDLL_SLICE_SKIPPER_H */
