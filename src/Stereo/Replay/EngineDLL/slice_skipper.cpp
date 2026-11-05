/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "Analysis/APIDumpDependencies/frame_slice_planner.h"
#include "slice_skipper.h"

Stereo::Replay::EngineDLL::SliceSkipper::SliceSkipper()
    :m_slice_ptr(nullptr)
{
    /* Stub */
}

void Stereo::Replay::EngineDLL::SliceSkipper::clear_slice()
{
    m_slice_ptr = nullptr;
}

Capture::APIDumpLoader::ChunkWantedFunc Stereo::Replay::EngineDLL::SliceSkipper::get_chunk_wanted_func() const
{
    const Analysis::APIDumpDependencies::FrameSlice* const slice_ptr = m_slice_ptr;
    Capture::APIDumpLoader::ChunkWantedFunc                result;

    /* A slice is never freed, so the function may outlive the skipper's use of it; it is the same decision as should_execute_call() makes. */
    if (slice_ptr != nullptr)
    {
        result = [slice_ptr](const Capture::APIDumpLoader::DumpedAPICall& in_call,
                             const uint64_t&                              in_frame_index)
                 {
                     return (Analysis::APIDumpDependencies::FrameSlicePlanner::is_live(*slice_ptr,
                                                                                       in_frame_index) ||
                             is_content_call(in_call.func)                                              == false);
                 };
    }

    return result;
}

bool Stereo::Replay::EngineDLL::SliceSkipper::is_content_call(const APIInterceptor::APIFunction& in_function)
{
    bool result = false;

    switch (in_function)
    {
        case APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_CLEAR:
        case APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_DRAWINDEXEDPRIMITIVE:
        case APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_DRAWINDEXEDPRIMITIVESTRIDED:
        case APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_DRAWINDEXEDPRIMITIVEVB:
        case APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_DRAWPRIMITIVE:
        case APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_DRAWPRIMITIVESTRIDED:
        case APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_DRAWPRIMITIVEVB:
        case APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_LOAD:
        case APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DVERTEXBUFFER7_LOCK:
        case APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DVERTEXBUFFER7_UNLOCK:
        case APIInterceptor::APIFUNCTION_D3D7_IDIRECTDRAWSURFACE7_BLT:
        case APIInterceptor::APIFUNCTION_D3D7_IDIRECTDRAWSURFACE7_BLTFAST:
        case APIInterceptor::APIFUNCTION_D3D7_IDIRECTDRAWSURFACE7_LOCK:
        case APIInterceptor::APIFUNCTION_D3D7_IDIRECTDRAWSURFACE7_UNLOCK:
        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_CLEAR:
        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_COPYRECTS:
        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_DRAWINDEXEDPRIMITIVE:
        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_DRAWINDEXEDPRIMITIVEUP:
        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_DRAWPRIMITIVE:
        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_DRAWPRIMITIVEUP:
        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DINDEXBUFFER8_LOCK:
        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DINDEXBUFFER8_UNLOCK:
        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DSURFACE8_LOCKRECT:
        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DSURFACE8_UNLOCKRECT:
        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DTEXTURE8_LOCKRECT:
        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DTEXTURE8_UNLOCKRECT:
        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DVERTEXBUFFER8_LOCK:
        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DVERTEXBUFFER8_UNLOCK:
        {
            result = true;

            break;
        }

        default:
        {
            break;
        }
    }

    return result;
}

bool Stereo::Replay::EngineDLL::SliceSkipper::is_dead_frame(const uint32_t& in_n_frame) const
{
    const Analysis::APIDumpDependencies::FrameSlice* const slice_ptr = m_slice_ptr;

    return (slice_ptr != nullptr &&
            Analysis::APIDumpDependencies::FrameSlicePlanner::is_live(*slice_ptr,
                                                                      in_n_frame) == false);
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
    const Analysis::APIDumpDependencies::FrameSlice* const slice_ptr = m_slice_ptr;

    return (slice_ptr == nullptr                                                                 ||
            Analysis::APIDumpDependencies::FrameSlicePlanner::is_live(*slice_ptr,
                                                                      in_n_frame)                ||
            is_content_call(in_call.func)                                                        == false);
}
