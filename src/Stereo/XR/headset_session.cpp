/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "headset_session_impl.h"

Stereo::XR::HeadsetSession::HeadsetSession(std::unique_ptr<Impl> in_impl_ptr)
    :m_impl_ptr(std::move(in_impl_ptr) )
{
    /* Stub */
}

Stereo::XR::HeadsetSession::~HeadsetSession()
{
    /* Stub */
}

Stereo::XR::HeadsetSession::Impl::~Impl()
{
    destroy_swapchains();

    if (action != XR_NULL_HANDLE)
    {
        ::xrDestroyAction(action);
    }

    if (action_set != XR_NULL_HANDLE)
    {
        ::xrDestroyActionSet(action_set);
    }

    if (view_space != XR_NULL_HANDLE)
    {
        ::xrDestroySpace(view_space);
    }

    if (local_space != XR_NULL_HANDLE)
    {
        ::xrDestroySpace(local_space);
    }

    if (session != XR_NULL_HANDLE)
    {
        ::xrDestroySession(session);
    }

    if (instance != XR_NULL_HANDLE)
    {
        ::xrDestroyInstance(instance);
    }
}

void Stereo::XR::HeadsetSession::Impl::add_log(const std::string& in_text)
{
    std::lock_guard<std::mutex> lock(log_mutex);

    log_line_vec.push_back(in_text);
}

std::unique_ptr<Stereo::XR::HeadsetSession> Stereo::XR::HeadsetSession::create_ptr(std::string* out_error_ptr)
{
    std::unique_ptr<Impl>           impl_ptr(new Impl() );
    std::unique_ptr<HeadsetSession> result_ptr;

    if (impl_ptr->set_up(out_error_ptr) == false)
    {
        goto end;
    }

    result_ptr.reset(new HeadsetSession(std::move(impl_ptr) ));

end:
    return result_ptr;
}

ID3D12Device* Stereo::XR::HeadsetSession::get_device_ptr() const
{
    return m_impl_ptr->device_ptr.Get();
}

ID3D12CommandQueue* Stereo::XR::HeadsetSession::get_queue_ptr() const
{
    return m_impl_ptr->queue_ptr.Get();
}

std::string Stereo::XR::HeadsetSession::Impl::get_result_text(const XrResult& in_result) const
{
    char text_i8vec64[XR_MAX_RESULT_STRING_SIZE] = {};

    ::xrResultToString(instance,
                       in_result,
                       text_i8vec64);

    return text_i8vec64;
}

const char* Stereo::XR::HeadsetSession::Impl::get_state_name(const XrSessionState& in_state)
{
    const char* result_ptr = "of a state which is not known";

    switch (in_state)
    {
        case XR_SESSION_STATE_EXITING:      result_ptr = "exiting";      break;
        case XR_SESSION_STATE_FOCUSED:      result_ptr = "focused";      break;
        case XR_SESSION_STATE_IDLE:         result_ptr = "idle";         break;
        case XR_SESSION_STATE_LOSS_PENDING: result_ptr = "loss pending"; break;
        case XR_SESSION_STATE_READY:        result_ptr = "ready";        break;
        case XR_SESSION_STATE_STOPPING:     result_ptr = "stopping";     break;
        case XR_SESSION_STATE_SYNCHRONIZED: result_ptr = "synchronized"; break;
        case XR_SESSION_STATE_VISIBLE:      result_ptr = "visible";      break;

        default:
        {
            break;
        }
    }

    return result_ptr;
}

float Stereo::XR::HeadsetSession::get_ipd() const
{
    return m_impl_ptr->ipd;
}

std::string Stereo::XR::HeadsetSession::get_system_name() const
{
    return m_impl_ptr->system_name;
}

D3D12_RESOURCE_STATES Stereo::XR::HeadsetSession::get_texture_state()
{
    return D3D12_RESOURCE_STATE_RENDER_TARGET;
}

bool Stereo::XR::HeadsetSession::probe(HeadsetInfo* out_info_ptr,
                                       std::string* out_error_ptr)
{
    Impl impl;
    bool result = false;

    if (impl.create_instance(out_error_ptr) == false)
    {
        goto end;
    }

    if (impl.find_system(out_error_ptr) == false)
    {
        goto end;
    }

    out_info_ptr->runtime_name = impl.runtime_name;
    out_info_ptr->system_name  = impl.system_name;

    /* The size of the image of an eye that the runtime recommends: what the headset is meant to be rendered at. */
    {
        uint32_t                n_views       = 0;
        XrViewConfigurationView view_array[2] = {{XR_TYPE_VIEW_CONFIGURATION_VIEW}, {XR_TYPE_VIEW_CONFIGURATION_VIEW}};

        if (XR_SUCCEEDED(::xrEnumerateViewConfigurationViews(impl.instance,
                                                             impl.system_id,
                                                             XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
                                                             2,
                                                            &n_views,
                                                             view_array) ) &&
            n_views > 0)
        {
            out_info_ptr->eye_height = view_array[0].recommendedImageRectHeight;
            out_info_ptr->eye_width  = view_array[0].recommendedImageRectWidth;
        }
    }

    result = true;

end:
    return result;
}

void Stereo::XR::HeadsetSession::request_exit()
{
    m_impl_ptr->is_exit_requested = true;

    ::xrRequestExitSession(m_impl_ptr->session);
}

std::vector<std::string> Stereo::XR::HeadsetSession::take_log_lines()
{
    std::lock_guard<std::mutex> lock(m_impl_ptr->log_mutex);

    std::vector<std::string> result_vec;

    result_vec.swap(m_impl_ptr->log_line_vec);

    return result_vec;
}

void Stereo::XR::HeadsetSession::set_frame_func(const FrameFunc& in_frame_func)
{
    m_impl_ptr->frame_func = in_frame_func;
}
