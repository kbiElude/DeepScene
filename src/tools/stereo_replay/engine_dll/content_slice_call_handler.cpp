/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "capture/api_dump_loader/call_describer_set.h"
#include "content_slice_call_handler.h"

Stereo::Replay::EngineDLL::ContentSliceCallHandler::ContentSliceCallHandler(const Capture::APIDumpLoader::APIGroup& in_group)
    :m_group(in_group)
{
    /* Stub */
}

Capture::APIDumpLoader::CallKeptFunc Stereo::Replay::EngineDLL::ContentSliceCallHandler::create_call_kept_func(const FrameLiveFunc& in_is_live_func) const
{
    (void) in_is_live_func;

    return Capture::APIDumpLoader::CallKeptFunc();
}

Capture::APIDumpLoader::APIGroup Stereo::Replay::EngineDLL::ContentSliceCallHandler::get_group() const
{
    return m_group;
}

bool Stereo::Replay::EngineDLL::ContentSliceCallHandler::is_chunk_wanted_in_dead_frame(const Capture::APIDumpLoader::DumpedAPICall& in_call) const
{
    const auto call_class = Capture::APIDumpLoader::CallDescriberSet::get_function_class(in_call.func);

    return (call_class != Capture::APIDumpLoader::CallClass::CONTENT);
}

bool Stereo::Replay::EngineDLL::ContentSliceCallHandler::should_execute_call(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                             const bool&                                  in_is_dead_frame)
{
    const auto call_class = Capture::APIDumpLoader::CallDescriberSet::get_function_class(in_call.func);

    return (in_is_dead_frame == false                                        ||
            call_class       != Capture::APIDumpLoader::CallClass::CONTENT);
}
