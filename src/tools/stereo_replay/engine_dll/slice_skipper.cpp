/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_dependencies/frame_slice_planner.h"
#include "analysis/api_dump_index/call_describer_set.h"
#include "capture/api_dump_loader/api_info.h"
#include "content_slice_call_handler.h"
#include "gl_slice_call_handler.h"
#include "slice_skipper.h"

Stereo::Replay::EngineDLL::SliceSkipper::SliceSkipper()
    :m_slice_ptr(nullptr)
{
    m_handler_ptr_vec.emplace_back(new ContentSliceCallHandler(Capture::APIDumpLoader::APIGroup::D3D7) );
    m_handler_ptr_vec.emplace_back(new ContentSliceCallHandler(Capture::APIDumpLoader::APIGroup::D3D8) );
    m_handler_ptr_vec.emplace_back(new GLSliceCallHandler() );

    m_handler_by_group_ptr_arr.fill(nullptr);

    for (const auto& handler_ptr : m_handler_ptr_vec)
    {
        m_handler_by_group_ptr_arr.at(static_cast<size_t>(handler_ptr->get_group() ) ) = handler_ptr.get();
    }
}

void Stereo::Replay::EngineDLL::SliceSkipper::clear_slice()
{
    m_slice_ptr = nullptr;
}

Stereo::Replay::EngineDLL::ISliceCallHandler* Stereo::Replay::EngineDLL::SliceSkipper::find_handler_ptr(const Capture::APIDumpLoader::APIGroup& in_group) const
{
    return m_handler_by_group_ptr_arr.at(static_cast<size_t>(in_group) );
}

std::vector<Capture::APIDumpLoader::APIGroup> Stereo::Replay::EngineDLL::SliceSkipper::get_api_groups() const
{
    std::vector<Capture::APIDumpLoader::APIGroup> result_vec;

    for (const auto& handler_ptr : m_handler_ptr_vec)
    {
        result_vec.push_back(handler_ptr->get_group() );
    }

    return result_vec;
}

Capture::APIDumpLoader::CallKeptFuncFactory Stereo::Replay::EngineDLL::SliceSkipper::get_call_kept_func_factory() const
{
    Capture::APIDumpLoader::CallKeptFuncFactory      result;
    const Analysis::APIDumpDependencies::FrameSlice* const slice_ptr = m_slice_ptr;

    /* A slice is never freed (see get_chunk_wanted_func()), so the lambda can keep a pointer to it. The handlers live as long as the skipper. */
    if (slice_ptr != nullptr)
    {
        std::vector<ISliceCallHandler*> handler_ptr_vec;

        for (const auto& handler_ptr : m_handler_ptr_vec)
        {
            handler_ptr_vec.push_back(handler_ptr.get() );
        }

        result = [slice_ptr,
                  handler_ptr_vec]()
                 {
                     typedef std::pair<Capture::APIDumpLoader::APIGroup, Capture::APIDumpLoader::CallKeptFunc> GroupFunc;

                     const FrameLiveFunc is_live_func = [slice_ptr](const uint64_t& in_frame_index)
                     {
                         return Analysis::APIDumpDependencies::FrameSlicePlanner::is_live(*slice_ptr,
                                                                                          in_frame_index);
                     };

                     std::vector<GroupFunc> group_func_vec;

                     /* Each function of a segment keeps its own state, because the threads that read segments work independently of each other. */
                     for (const auto handler_ptr : handler_ptr_vec)
                     {
                         auto func = handler_ptr->create_call_kept_func(is_live_func);

                         if (func)
                         {
                             group_func_vec.emplace_back(handler_ptr->get_group(),
                                                         func);
                         }
                     }

                     return Capture::APIDumpLoader::CallKeptFunc([group_func_vec](const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                                  const uint64_t&                              in_frame_index) mutable
                                                                 {
                                                                     const auto group   = Capture::APIDumpLoader::get_api_group(in_call.func);
                                                                     bool       is_kept = true;

                                                                     for (auto& group_func : group_func_vec)
                                                                     {
                                                                         if (group_func.first == group)
                                                                         {
                                                                             is_kept = group_func.second(in_call,
                                                                                                         in_frame_index);

                                                                             break;
                                                                         }
                                                                     }

                                                                     return is_kept;
                                                                 });
                 };
    }

    return result;
}

Capture::APIDumpLoader::ChunkWantedFunc Stereo::Replay::EngineDLL::SliceSkipper::get_chunk_wanted_func() const
{
    Capture::APIDumpLoader::ChunkWantedFunc                result;
    const Analysis::APIDumpDependencies::FrameSlice* const slice_ptr = m_slice_ptr;

    /* A slice is never freed, so the function can outlive this call. It decides like should_execute_call(), except for what a handler says
     * it needs the chunks of.
     */
    if (slice_ptr != nullptr)
    {
        std::vector<ISliceCallHandler*> handler_ptr_vec;

        for (const auto& handler_ptr : m_handler_ptr_vec)
        {
            handler_ptr_vec.push_back(handler_ptr.get() );
        }

        result = [slice_ptr,
                  handler_ptr_vec](const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                   const uint64_t&                              in_frame_index)
                 {
                     const bool is_slice_live = Analysis::APIDumpDependencies::FrameSlicePlanner::is_live(*slice_ptr,
                                                                                                          in_frame_index);
                     bool       is_wanted     = true;

                     if (is_slice_live == false)
                     {
                         const auto group = Capture::APIDumpLoader::get_api_group(in_call.func);

                         for (const auto handler_ptr : handler_ptr_vec)
                         {
                             if (handler_ptr->get_group() == group)
                             {
                                 is_wanted = handler_ptr->is_chunk_wanted_in_dead_frame(in_call);

                                 break;
                             }
                         }
                     }

                     return is_wanted;
                 };
    }

    return result;
}

Capture::APIDumpLoader::StateOnlyFunc Stereo::Replay::EngineDLL::SliceSkipper::get_state_only_func() const
{
    Capture::APIDumpLoader::StateOnlyFunc                  result;
    const Analysis::APIDumpDependencies::FrameSlice* const slice_ptr = m_slice_ptr;

    /* A slice is never freed, so the function can outlive this call. */
    if (slice_ptr != nullptr)
    {
        result = [slice_ptr](const uint64_t& in_first_frame_index,
                             const uint64_t& in_last_frame_index)
                 {
                     bool is_all_dead = true;

                     for (uint64_t n_frame = in_first_frame_index;
                                   n_frame <= in_last_frame_index &&
                                   is_all_dead;
                                 ++n_frame)
                     {
                         is_all_dead = (Analysis::APIDumpDependencies::FrameSlicePlanner::is_live(*slice_ptr,
                                                                                                 n_frame) == false);
                     }

                     return is_all_dead;
                 };
    }

    return result;
}

bool Stereo::Replay::EngineDLL::SliceSkipper::is_content_call(const APIInterceptor::APIFunction& in_function)
{
    return (Analysis::APIDumpIndex::CallDescriberSet::get_function_class(in_function) != Analysis::APIDumpIndex::CallClass::STATE);
}

bool Stereo::Replay::EngineDLL::SliceSkipper::is_dead_frame(const uint32_t& in_n_frame) const
{
    const Analysis::APIDumpDependencies::FrameSlice* const slice_ptr = m_slice_ptr;

    bool result = false;

    if (slice_ptr != nullptr)
    {
        const bool is_slice_live = Analysis::APIDumpDependencies::FrameSlicePlanner::is_live(*slice_ptr,
                                                                                             in_n_frame);

        result = (is_slice_live == false);
    }

    return result;
}

void Stereo::Replay::EngineDLL::SliceSkipper::on_call_replayed(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                               const uint32_t&                              in_n_frame,
                                                               const bool&                                  in_was_executed)
{
    (void) in_call;
    (void) in_n_frame;
    (void) in_was_executed;
}

void Stereo::Replay::EngineDLL::SliceSkipper::set_slice(const Analysis::APIDumpDependencies::FrameSlice& in_slice)
{
    std::lock_guard<std::mutex> lock(m_slice_mutex);

    m_slice_ptr_deque.push_back(std::unique_ptr<Analysis::APIDumpDependencies::FrameSlice>(new Analysis::APIDumpDependencies::FrameSlice(in_slice) ) );

    m_slice_ptr = m_slice_ptr_deque.back().get();
}

bool Stereo::Replay::EngineDLL::SliceSkipper::should_execute_call(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                  const uint32_t&                              in_n_frame)
{
    const auto               group       = Capture::APIDumpLoader::get_api_group(in_call.func);
    ISliceCallHandler* const handler_ptr = find_handler_ptr(group);
    bool                     result      = true;

    if (handler_ptr != nullptr)
    {
        const bool is_dead = is_dead_frame(in_n_frame);

        result = handler_ptr->should_execute_call(in_call,
                                                  is_dead);
    }

    return result;
}
