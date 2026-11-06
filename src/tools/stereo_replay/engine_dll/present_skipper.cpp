/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "capture/api_dump_loader/api_info.h"
#include "present_skipper.h"

Stereo::Replay::EngineDLL::PresentSkipper::PresentSkipper(const FrameHurry* in_hurry_ptr)
    :m_hurry_ptr(in_hurry_ptr)
{
    /* Stub */
}

void Stereo::Replay::EngineDLL::PresentSkipper::on_call_replayed(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                 const uint32_t&                              in_n_frame,
                                                                 const bool&                                  in_was_executed)
{
    (void) in_call;
    (void) in_n_frame;
    (void) in_was_executed;
}

bool Stereo::Replay::EngineDLL::PresentSkipper::should_execute_call(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                    const uint32_t&                              in_n_frame)
{
    return (m_hurry_ptr->is_hurrying(in_n_frame)                  == false ||
            Capture::APIDumpLoader::is_frame_boundary(in_call) == false);
}
