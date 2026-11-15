/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "headset/xr/headset_session.h"
#include <chrono>
#include <math.h>
#include <stdio.h>
#include <string.h>

/* How far the screen is from the head when it is put: where eyes of a headset focus is about this. */
constexpr float SCREEN_DISTANCE = 2.0f;

Headset::XR::HeadsetSession::HeadsetSession()
{
    /* Stub */
}

Headset::XR::HeadsetSession::~HeadsetSession()
{
    destroy_swapchains();

    if (m_action != XR_NULL_HANDLE)
    {
        ::xrDestroyAction(m_action);
    }

    if (m_action_set != XR_NULL_HANDLE)
    {
        ::xrDestroyActionSet(m_action_set);
    }

    if (m_view_space != XR_NULL_HANDLE)
    {
        ::xrDestroySpace(m_view_space);
    }

    if (m_local_space != XR_NULL_HANDLE)
    {
        ::xrDestroySpace(m_local_space);
    }

    if (m_session != XR_NULL_HANDLE)
    {
        ::xrDestroySession(m_session);
    }

    if (m_instance != XR_NULL_HANDLE)
    {
        ::xrDestroyInstance(m_instance);
    }
}

bool Headset::XR::HeadsetSession::acquire_eye_textures(const uint32_t&  in_width,
                                                       const uint32_t&  in_height,
                                                       ID3D12Resource** out_left_texture_ptr_ptr,
                                                       ID3D12Resource** out_right_texture_ptr_ptr,
                                                       std::string*     out_error_ptr)
{
    constexpr int64_t WAIT_N_NANOSECONDS = 8 * 1000 * 1000; /* the compositor gives an image back within a frame, which is 14 ms at 72 Hz */

    bool result = false;

    {
        std::lock_guard<std::mutex> lock(m_shared.mutex);

        if (m_shared.is_running == false)
        {
            *out_error_ptr = "The session of the headset is not running";

            goto end;
        }

        if (in_width  > m_max_width ||
            in_height > m_max_height)
        {
            *out_error_ptr = "An image of an eye ("                 +
                             std::to_string(in_width)               +
                             " x "                                  +
                             std::to_string(in_height)              +
                             ") is larger than the headset takes (" +
                             std::to_string(m_max_width)            +
                             " x "                                  +
                             std::to_string(m_max_height)           +
                             ")";

            goto end;
        }

        if (make_swapchains(in_width,
                            in_height,
                            out_error_ptr) == false)
        {
            goto end;
        }

        for (auto& eye : m_shared.eye_array)
        {
            XrResult wait_result = XR_SUCCESS;

            if (eye.state == SwapchainImageState::IDLE)
            {
                XrSwapchainImageAcquireInfo acquire_info = {XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};

                if (XR_FAILED(::xrAcquireSwapchainImage(eye.swapchain,
                                                       &acquire_info,
                                                       &eye.acquired_index) ))
                {
                    *out_error_ptr = "An image of the swap chain cannot be taken";

                    goto end;
                }

                eye.state = SwapchainImageState::ACQUIRED;
            }

            if (eye.state == SwapchainImageState::ACQUIRED)
            {
                XrSwapchainImageWaitInfo wait_info = {XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};

                wait_info.timeout = WAIT_N_NANOSECONDS;
                wait_result       = ::xrWaitSwapchainImage(eye.swapchain,
                                                          &wait_info);

                if (wait_result == XR_TIMEOUT_EXPIRED)
                {
                    *out_error_ptr = "The compositor has not given an image back yet";

                    goto end;
                }

                if (XR_FAILED(wait_result) )
                {
                    *out_error_ptr = "An image of the swap chain cannot be waited for";

                    goto end;
                }

                eye.state = SwapchainImageState::READY;
            }
        }

        *out_left_texture_ptr_ptr  = m_shared.eye_array.at(0).image_vec.at(m_shared.eye_array.at(0).acquired_index).texture;
        *out_right_texture_ptr_ptr = m_shared.eye_array.at(1).image_vec.at(m_shared.eye_array.at(1).acquired_index).texture;

        result = true;
    }

end:
    return result;
}

void Headset::XR::HeadsetSession::add_log(const std::string& in_text)
{
    std::lock_guard<std::mutex> lock(m_log.mutex);

    m_log.line_vec.push_back(in_text);
}

bool Headset::XR::HeadsetSession::clear_swapchain_images(std::string* out_error_ptr)
{
    constexpr int64_t WAIT_N_NANOSECONDS = 100 * 1000 * 1000; /* a swap chain that was just made gives its images back at once */

    const D3D12_RESOURCE_STATES texture_state = HeadsetSession::get_texture_state();
    const DXGI_FORMAT           view_format   = static_cast<DXGI_FORMAT>(m_format);
    bool                        result        = true;

    if (m_texture_clearer_ptr == nullptr)
    {
        m_texture_clearer_ptr.reset(new Utils::D3D12::D3D12TextureClearer(m_device_ptr.Get(),
                                                                          m_queue_ptr.Get() ));
    }

    if (m_texture_clearer_ptr->is_ok(out_error_ptr) == false)
    {
        result = false;

        goto end;
    }

    for (auto& eye : m_shared.eye_array)
    {
        for (size_t n_image = 0;
                    n_image < eye.image_vec.size();
                  ++n_image)
        {
            XrSwapchainImageAcquireInfo acquire_info = {XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
            uint32_t                    image_index  = 0;
            XrSwapchainImageReleaseInfo release_info = {XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
            XrSwapchainImageWaitInfo    wait_info    = {XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};

            wait_info.timeout = WAIT_N_NANOSECONDS;

            if (XR_FAILED(::xrAcquireSwapchainImage(eye.swapchain,
                                                   &acquire_info,
                                                   &image_index) ))
            {
                *out_error_ptr = "An image of the swap chain cannot be taken";
                result         = false;

                goto end;
            }

            if (::xrWaitSwapchainImage(eye.swapchain,
                                      &wait_info) != XR_SUCCESS)
            {
                *out_error_ptr = "An image of the swap chain cannot be waited for";
                result         = false;

                goto end;
            }

            /* An image that was taken is given back whether or not it could be made black. */
            {
                ID3D12Resource* const texture_ptr = eye.image_vec.at(image_index).texture;

                if (m_texture_clearer_ptr->clear_to_black(texture_ptr,
                                                          view_format,
                                                          texture_state,
                                                          out_error_ptr) == false)
                {
                    result = false;
                }
            }

            if (XR_FAILED(::xrReleaseSwapchainImage(eye.swapchain,
                                                   &release_info) ))
            {
                *out_error_ptr = "An image of the swap chain cannot be given back";
                result         = false;

                goto end;
            }
        }

        /* Black is shown only if every image of the eye is. */
        if (result)
        {
            eye.has_image = true;
        }
    }

end:
    return result;
}

bool Headset::XR::HeadsetSession::create_actions(std::string* out_error_ptr)
{
    /* The button of each hand that every controller has (the simple controller), and the one of a Touch controller which is for the
     * app (the menu button of the left one, A of the right one). A runtime which does not know a profile does not use what is
     * suggested for it.
     */
    struct ProfileBinding
    {
        const char* interaction_profile_ptr;
        const char* path_a_ptr;
        const char* path_b_ptr;
    };

    const ProfileBinding profile_binding_array[] =
    {
        {"/interaction_profiles/khr/simple_controller",   "/user/hand/left/input/menu/click", "/user/hand/right/input/menu/click"},
        {"/interaction_profiles/oculus/touch_controller", "/user/hand/left/input/menu/click", "/user/hand/right/input/a/click"}
    };

    XrActionCreateInfo            action_info     = {XR_TYPE_ACTION_CREATE_INFO};
    XrActionSetCreateInfo         action_set_info = {XR_TYPE_ACTION_SET_CREATE_INFO};
    XrSessionActionSetsAttachInfo attach_info     = {XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};
    bool                          result          = false;

    ::strncpy_s(action_set_info.actionSetName,
                "stereo_host",
                _TRUNCATE);
    ::strncpy_s(action_set_info.localizedActionSetName,
                "Stereo host",
                _TRUNCATE);

    ::strncpy_s(action_info.actionName,
                "recenter",
                _TRUNCATE);
    ::strncpy_s(action_info.localizedActionName,
                "Put the screen where you look",
                _TRUNCATE);

    action_info.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;

    if (XR_FAILED(::xrCreateActionSet(m_instance,
                                     &action_set_info,
                                     &m_action_set) ))
    {
        *out_error_ptr = "The action to put the screen where the user looks cannot be made";

        goto end;
    }

    if (XR_FAILED(::xrCreateAction(m_action_set,
                                  &action_info,
                                  &m_action) ))
    {
        *out_error_ptr = "The action to put the screen where the user looks cannot be made";

        goto end;
    }

    for (const auto& profile_binding : profile_binding_array)
    {
        XrActionSuggestedBinding             binding_array[2]  = {};
        XrInteractionProfileSuggestedBinding suggested_binding = {XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};

        binding_array[0].action = m_action;
        binding_array[1].action = m_action;

        if (XR_FAILED(::xrStringToPath(m_instance,
                                       profile_binding.path_a_ptr,
                                      &binding_array[0].binding) ))
        {
            continue;
        }

        if (XR_FAILED(::xrStringToPath(m_instance,
                                       profile_binding.path_b_ptr,
                                      &binding_array[1].binding) ))
        {
            continue;
        }

        if (XR_FAILED(::xrStringToPath(m_instance,
                                       profile_binding.interaction_profile_ptr,
                                      &suggested_binding.interactionProfile) ))
        {
            continue;
        }

        suggested_binding.countSuggestedBindings = 2;
        suggested_binding.suggestedBindings      = binding_array;

        /* Not an error: what a runtime does not know of is not used. */
        ::xrSuggestInteractionProfileBindings(m_instance,
                                             &suggested_binding);
    }

    attach_info.actionSets      = &m_action_set;
    attach_info.countActionSets = 1;

    if (XR_FAILED(::xrAttachSessionActionSets(m_session,
                                             &attach_info) ))
    {
        *out_error_ptr = "The action cannot be given to the session";

        goto end;
    }

    result = true;

end:
    return result;
}

bool Headset::XR::HeadsetSession::create_device(std::string* out_error_ptr)
{
    Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter_ptr;
    Microsoft::WRL::ComPtr<IDXGIFactory4> factory_ptr;
    PFN_xrGetD3D12GraphicsRequirementsKHR get_requirements_func_ptr = nullptr;
    D3D12_COMMAND_QUEUE_DESC              queue_desc                = {};
    XrGraphicsRequirementsD3D12KHR        requirements              = {XR_TYPE_GRAPHICS_REQUIREMENTS_D3D12_KHR};
    bool                                  result                    = false;

    if (XR_FAILED(::xrGetInstanceProcAddr(m_instance,
                                          "xrGetD3D12GraphicsRequirementsKHR",
                                          reinterpret_cast<PFN_xrVoidFunction*>(&get_requirements_func_ptr) )))
    {
        *out_error_ptr = "The headset does not say which adapter it is on";

        goto end;
    }

    if (XR_FAILED(get_requirements_func_ptr(m_instance,
                                            m_system_id,
                                           &requirements) ))
    {
        *out_error_ptr = "The headset does not say which adapter it is on";

        goto end;
    }

    if (FAILED(::CreateDXGIFactory1(IID_PPV_ARGS(factory_ptr.GetAddressOf() ))))
    {
        *out_error_ptr = "The adapter that the headset is on cannot be found";

        goto end;
    }

    if (FAILED(factory_ptr->EnumAdapterByLuid(requirements.adapterLuid,
                                              IID_PPV_ARGS(adapter_ptr.GetAddressOf() ))))
    {
        *out_error_ptr = "The adapter that the headset is on cannot be found";

        goto end;
    }

    if (FAILED(::D3D12CreateDevice(adapter_ptr.Get(),
                                   requirements.minFeatureLevel,
                                   IID_PPV_ARGS(m_device_ptr.GetAddressOf() ))))
    {
        *out_error_ptr = "The device for the adapter that the headset is on cannot be made";

        goto end;
    }

    queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

    if (FAILED(m_device_ptr->CreateCommandQueue(&queue_desc,
                                                IID_PPV_ARGS(m_queue_ptr.GetAddressOf() ))))
    {
        *out_error_ptr = "The command queue for the headset cannot be made";

        goto end;
    }

    result = true;

end:
    return result;
}

bool Headset::XR::HeadsetSession::create_instance(std::string* out_error_ptr)
{
    /* What is asked for: the way to give the compositor textures of Direct3D 12, and, if it has it, the curved screen. */
    std::vector<const char*>           enabled_extension_vec = {XR_KHR_D3D12_ENABLE_EXTENSION_NAME};
    std::vector<XrExtensionProperties> extension_vec;
    bool                               has_d3d12             = false;
    uint32_t                           n_extensions          = 0;
    bool                               result                = false;
    XrResult                           xr_result             = XR_SUCCESS;

    ::xrEnumerateInstanceExtensionProperties(nullptr, /* in_layer_name_ptr */
                                             0,
                                            &n_extensions,
                                             nullptr);

    extension_vec.resize(n_extensions,
                         XrExtensionProperties{XR_TYPE_EXTENSION_PROPERTIES});

    xr_result = ::xrEnumerateInstanceExtensionProperties(nullptr, /* in_layer_name_ptr */
                                                         n_extensions,
                                                        &n_extensions,
                                                         extension_vec.data() );

    if (XR_FAILED(xr_result) )
    {
        *out_error_ptr = (xr_result == XR_ERROR_RUNTIME_UNAVAILABLE) ? "There is no OpenXR runtime which can be started: none is installed, or the active one cannot be found "
                                                                       "(for a Quest, set Meta Quest Link as the active OpenXR runtime in its settings; result " +
                                                                       std::to_string(static_cast<int>(xr_result))                                               +
                                                                       ")"
                                                                     : "There is no OpenXR runtime (its extensions cannot be listed; result " +
                                                                       std::to_string(static_cast<int>(xr_result))                            +
                                                                       ")";

        goto end;
    }

    for (const auto& extension : extension_vec)
    {
        const int cylinder_difference = ::strcmp(extension.extensionName,
                                                 XR_KHR_COMPOSITION_LAYER_CYLINDER_EXTENSION_NAME);
        const int d3d12_difference    = ::strcmp(extension.extensionName,
                                                 XR_KHR_D3D12_ENABLE_EXTENSION_NAME);

        has_d3d12             = (has_d3d12             || d3d12_difference    == 0);
        m_is_cylinder_supported = (m_is_cylinder_supported || cylinder_difference == 0);
    }

    if (has_d3d12 == false)
    {
        *out_error_ptr = "The OpenXR runtime does not take textures of Direct3D 12 (it has no " XR_KHR_D3D12_ENABLE_EXTENSION_NAME ")";

        goto end;
    }

    if (m_is_cylinder_supported)
    {
        enabled_extension_vec.push_back(XR_KHR_COMPOSITION_LAYER_CYLINDER_EXTENSION_NAME);
    }

    {
        XrInstanceCreateInfo instance_info = {XR_TYPE_INSTANCE_CREATE_INFO};

        ::strncpy_s(instance_info.applicationInfo.applicationName,
                    "StereoPresenter",
                    _TRUNCATE);
        ::strncpy_s(instance_info.applicationInfo.engineName,
                    "APIInterceptor",
                    _TRUNCATE);

        instance_info.applicationInfo.apiVersion         = XR_API_VERSION_1_0;
        instance_info.applicationInfo.applicationVersion = 1;
        instance_info.applicationInfo.engineVersion      = 1;
        instance_info.enabledExtensionCount              = static_cast<uint32_t>(enabled_extension_vec.size() );
        instance_info.enabledExtensionNames              = enabled_extension_vec.data();

        xr_result = ::xrCreateInstance(&instance_info,
                                       &m_instance);
    }

    if (XR_FAILED(xr_result) )
    {
        *out_error_ptr = "The OpenXR runtime cannot be started (result " +
                         std::to_string(static_cast<int>(xr_result))     +
                         ")";

        goto end;
    }

    {
        XrInstanceProperties instance_properties = {XR_TYPE_INSTANCE_PROPERTIES};

        if (XR_SUCCEEDED(::xrGetInstanceProperties(m_instance,
                                                  &instance_properties) ))
        {
            m_runtime_name = instance_properties.runtimeName;
        }
    }

    result = true;

end:
    return result;
}

std::unique_ptr<Headset::XR::HeadsetSession> Headset::XR::HeadsetSession::create_ptr(std::string* out_error_ptr)
{
    std::unique_ptr<HeadsetSession> result_ptr(new HeadsetSession() );

    if (result_ptr->set_up(out_error_ptr) == false)
    {
        result_ptr.reset();
    }

    return result_ptr;
}

bool Headset::XR::HeadsetSession::create_session(std::string* out_error_ptr)
{
    XrGraphicsBindingD3D12KHR binding      = {XR_TYPE_GRAPHICS_BINDING_D3D12_KHR};
    bool                      result       = false;
    XrSessionCreateInfo       session_info = {XR_TYPE_SESSION_CREATE_INFO};
    XrResult                  xr_result    = XR_SUCCESS;

    binding.device = m_device_ptr.Get();
    binding.queue  = m_queue_ptr.Get();

    session_info.next     = &binding;
    session_info.systemId = m_system_id;

    xr_result = ::xrCreateSession(m_instance,
                                 &session_info,
                                 &m_session);

    if (XR_FAILED(xr_result) )
    {
        *out_error_ptr = "The session of the headset cannot be made (" + get_result_text(xr_result) + ")";

        goto end;
    }

    result = true;

end:
    return result;
}

bool Headset::XR::HeadsetSession::create_spaces(std::string* out_error_ptr)
{
    bool                       result     = false;
    XrReferenceSpaceCreateInfo space_info = {XR_TYPE_REFERENCE_SPACE_CREATE_INFO};

    space_info.poseInReferenceSpace.orientation.w = 1.0f;
    space_info.referenceSpaceType                 = XR_REFERENCE_SPACE_TYPE_LOCAL;

    if (XR_FAILED(::xrCreateReferenceSpace(m_session,
                                          &space_info,
                                          &m_local_space) ))
    {
        *out_error_ptr = "The space of the room cannot be made";

        goto end;
    }

    space_info.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;

    if (XR_FAILED(::xrCreateReferenceSpace(m_session,
                                          &space_info,
                                          &m_view_space) ))
    {
        *out_error_ptr = "The space of the head cannot be made";

        goto end;
    }

    result = true;

end:
    return result;
}

void Headset::XR::HeadsetSession::destroy_swapchains()
{
    for (auto& eye : m_shared.eye_array)
    {
        if (eye.swapchain != XR_NULL_HANDLE)
        {
            ::xrDestroySwapchain(eye.swapchain);
        }

        eye = EyeSwapchain();
    }

    m_swapchain_height = 0;
    m_swapchain_width  = 0;
}

void Headset::XR::HeadsetSession::end_frame(const XrTime&                              in_time,
                                            const uint32_t&                            in_n_layers,
                                            const XrCompositionLayerBaseHeader* const* in_layer_ptr_array)
{
    XrFrameEndInfo end_info   = {XR_TYPE_FRAME_END_INFO};
    XrResult       end_result = XR_SUCCESS;

    end_info.displayTime          = in_time;
    end_info.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
    end_info.layerCount           = in_n_layers;
    end_info.layers               = in_layer_ptr_array;

    end_result = ::xrEndFrame(m_session,
                             &end_info);

    if (XR_FAILED(end_result) )
    {
        log_failure("xrEndFrame",
                    end_result);
    }
}

bool Headset::XR::HeadsetSession::find_system(std::string* out_error_ptr)
{
    bool            result      = false;
    XrSystemGetInfo system_info = {XR_TYPE_SYSTEM_GET_INFO};
    XrResult        xr_result   = XR_SUCCESS;

    system_info.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;

    xr_result = ::xrGetSystem(m_instance,
                             &system_info,
                             &m_system_id);

    if (XR_FAILED(xr_result) )
    {
        *out_error_ptr = "There is no headset ("    +
                         get_result_text(xr_result) +
                         ")";

        goto end;
    }

    {
        XrSystemProperties system_properties = {XR_TYPE_SYSTEM_PROPERTIES};

        if (XR_FAILED(::xrGetSystemProperties(m_instance,
                                              m_system_id,
                                             &system_properties) ))
        {
            *out_error_ptr = "The properties of the headset cannot be read";

            goto end;
        }

        m_max_height  = system_properties.graphicsProperties.maxSwapchainImageHeight;
        m_max_width   = system_properties.graphicsProperties.maxSwapchainImageWidth;
        m_system_name = system_properties.systemName;
    }

    result = true;

end:
    return result;
}

ID3D12Device* Headset::XR::HeadsetSession::get_device_ptr() const
{
    return m_device_ptr.Get();
}

float Headset::XR::HeadsetSession::get_ipd() const
{
    return m_ipd;
}

ID3D12CommandQueue* Headset::XR::HeadsetSession::get_queue_ptr() const
{
    return m_queue_ptr.Get();
}

std::string Headset::XR::HeadsetSession::get_result_text(const XrResult& in_result) const
{
    char text_i8vec64[XR_MAX_RESULT_STRING_SIZE] = {};

    ::xrResultToString(m_instance,
                       in_result,
                       text_i8vec64);

    return text_i8vec64;
}

const char* Headset::XR::HeadsetSession::get_state_name(const XrSessionState& in_state)
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

std::string Headset::XR::HeadsetSession::get_system_name() const
{
    return m_system_name;
}

D3D12_RESOURCE_STATES Headset::XR::HeadsetSession::get_texture_state()
{
    return D3D12_RESOURCE_STATE_RENDER_TARGET;
}

bool Headset::XR::HeadsetSession::locate_head(const XrTime& in_time)
{
    XrSpaceLocation location = {XR_TYPE_SPACE_LOCATION};
    bool            result   = false;

    const XrResult locate_result = ::xrLocateSpace(m_view_space,
                                                   m_local_space,
                                                   in_time,
                                                  &location);

    if (XR_SUCCEEDED(locate_result) )
    {
        if ((location.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT)    != 0 &&
            (location.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT) != 0)
        {
            m_screen_position_fp32vec3[0] = location.pose.position.x;
            m_screen_position_fp32vec3[1] = location.pose.position.y;
            m_screen_position_fp32vec3[2] = location.pose.position.z;
            m_screen_yaw                  = ScreenGeometry::get_yaw(location.pose.orientation.x,
                                                                    location.pose.orientation.y,
                                                                    location.pose.orientation.z,
                                                                    location.pose.orientation.w);

            result = true;
        }
    }

    return result;
}

void Headset::XR::HeadsetSession::log_failure(const char*     in_what_ptr,
                                              const XrResult& in_result)
{
    bool is_new = false;

    {
        std::lock_guard<std::mutex> lock(m_log.mutex);

        is_new = m_logged_result_set.insert(static_cast<int>(in_result) ).second;
    }

    if (is_new)
    {
        add_log(std::string(in_what_ptr) + " failed (" + get_result_text(in_result) + ")");
    }
}

void Headset::XR::HeadsetSession::log_screen(const float& in_aspect_ratio)
{
    constexpr float TO_DEGREES = 180.0f / 3.14159265f;

    const auto screen = ScreenGeometry::fit_cylinder(m_field_of_view,
                                                     in_aspect_ratio,
                                                     SCREEN_DISTANCE,
                                                     1.0f); /* in_scale */

    add_log(std::string("The screen is a ")                                                         +
            ((m_is_cylinder_supported) ? "cylinder" : "quad")                                       +
            " "                                                                                     +
            std::to_string(SCREEN_DISTANCE)                                                         +
            " m away, "                                                                             +
            std::to_string(screen.central_angle * TO_DEGREES)                                       +
            " degrees wide for a view of "                                                          +
            std::to_string((m_field_of_view.angle_right - m_field_of_view.angle_left) * TO_DEGREES) +
            " x "                                                                                   +
            std::to_string((m_field_of_view.angle_up - m_field_of_view.angle_down) * TO_DEGREES)    +
            " degrees, for an image of "                                                            +
            std::to_string(m_swapchain_width)                                                       +
            " x "                                                                                   +
            std::to_string(m_swapchain_height)                                                      +
            " per eye");

    m_has_logged_screen = true;
}

void Headset::XR::HeadsetSession::make_layer(const uint32_t& in_n_eye,
                                             const XrPosef&  in_pose,
                                             const float&    in_aspect_ratio)
{
    const XrEyeVisibility eye_visibility = (in_n_eye == 0) ? XR_EYE_VISIBILITY_LEFT
                                                           : XR_EYE_VISIBILITY_RIGHT;
    XrSwapchainSubImage   sub_image      = {};

    sub_image.imageRect.extent.height = static_cast<int32_t> (m_swapchain_height);
    sub_image.imageRect.extent.width  = static_cast<int32_t> (m_swapchain_width);
    sub_image.swapchain               = m_shared.eye_array.at(in_n_eye).swapchain;

    if (m_is_cylinder_supported)
    {
        const CylinderScreen           screen = ScreenGeometry::fit_cylinder(m_field_of_view,
                                                                             in_aspect_ratio,
                                                                             SCREEN_DISTANCE,
                                                                             1.0f); /* in_scale */
        XrCompositionLayerCylinderKHR& layer  = m_shared.cylinder_layer_array[in_n_eye];

        layer               = {XR_TYPE_COMPOSITION_LAYER_CYLINDER_KHR};
        layer.aspectRatio   = screen.aspect_ratio;
        layer.centralAngle  = screen.central_angle;
        layer.eyeVisibility = eye_visibility;
        layer.pose          = in_pose;
        layer.radius        = screen.radius;
        layer.space         = m_local_space;
        layer.subImage      = sub_image;
    }
    else
    {
        const QuadScreen        screen = ScreenGeometry::fit_quad(m_field_of_view,
                                                                  in_aspect_ratio,
                                                                  SCREEN_DISTANCE,
                                                                  1.0f); /* in_scale */
        XrCompositionLayerQuad& layer  = m_shared.quad_layer_array[in_n_eye];

        layer               = {XR_TYPE_COMPOSITION_LAYER_QUAD};
        layer.eyeVisibility = eye_visibility;
        layer.pose          = in_pose;
        layer.size.height   = screen.height;
        layer.size.width    = screen.width;
        layer.space         = m_local_space;
        layer.subImage      = sub_image;

        /* The quad is in front of the head, not around it. */
        layer.pose.position.x -= ::sinf(m_screen_yaw) * SCREEN_DISTANCE;
        layer.pose.position.z -= ::cosf(m_screen_yaw) * SCREEN_DISTANCE;
    }
}

uint32_t Headset::XR::HeadsetSession::make_layers(const XrTime& in_time)
{
    uint32_t result = 0;

    if (m_shared.eye_array.at(0).has_image == false ||
        m_shared.eye_array.at(1).has_image == false)
    {
        goto end;
    }

    update_field_of_view(in_time);

    if (m_has_field_of_view == false ||
        m_is_placed         == false)
    {
        goto end;
    }

    {
        const float aspect_ratio = static_cast<float>(m_swapchain_width) / static_cast<float>(m_swapchain_height);
        XrPosef     pose         = {};

        pose.orientation.w = ::cosf(m_screen_yaw / 2.0f);
        pose.orientation.y = ::sinf(m_screen_yaw / 2.0f);
        pose.position.x    = m_screen_position_fp32vec3[0];
        pose.position.y    = m_screen_position_fp32vec3[1];
        pose.position.z    = m_screen_position_fp32vec3[2];

        for (uint32_t n_eye = 0;
                      n_eye < 2;
                    ++n_eye)
        {
            make_layer(n_eye,
                       pose,
                       aspect_ratio);
        }

        if (m_has_logged_screen == false)
        {
            log_screen(aspect_ratio);
        }

        result = 2;
    }

end:
    return result;
}

bool Headset::XR::HeadsetSession::make_swapchains(const uint32_t& in_width,
                                                  const uint32_t& in_height,
                                                  std::string*    out_error_ptr)
{
    bool result = false;

    if (m_swapchain_width  == in_width  &&
        m_swapchain_height == in_height &&
        m_shared.eye_array.at(0).swapchain != XR_NULL_HANDLE)
    {
        result = true;

        goto end;
    }

    destroy_swapchains();

    for (auto& eye : m_shared.eye_array)
    {
        XrSwapchainCreateInfo swapchain_info = {XR_TYPE_SWAPCHAIN_CREATE_INFO};
        uint32_t              n_images       = 0;

        swapchain_info.arraySize   = 1;
        swapchain_info.faceCount   = 1;
        swapchain_info.format      = m_format;
        swapchain_info.height      = in_height;
        swapchain_info.mipCount    = 1;
        swapchain_info.sampleCount = 1;
        swapchain_info.usageFlags  = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT;
        swapchain_info.width       = in_width;

        if (XR_FAILED(::xrCreateSwapchain(m_session,
                                         &swapchain_info,
                                         &eye.swapchain) ))
        {
            *out_error_ptr = "The swap chain of an eye cannot be made";

            destroy_swapchains();

            goto end;
        }

        if (XR_FAILED(::xrEnumerateSwapchainImages(eye.swapchain,
                                                   0,
                                                  &n_images,
                                                   nullptr) ))
        {
            *out_error_ptr = "The swap chain of an eye cannot be made";

            destroy_swapchains();

            goto end;
        }

        eye.image_vec.resize(n_images,
                             XrSwapchainImageD3D12KHR{XR_TYPE_SWAPCHAIN_IMAGE_D3D12_KHR});

        if (XR_FAILED(::xrEnumerateSwapchainImages(eye.swapchain,
                                                   n_images,
                                                  &n_images,
                                                   reinterpret_cast<XrSwapchainImageBaseHeader*>(eye.image_vec.data() ))))
        {
            *out_error_ptr = "The images of the swap chain of an eye cannot be listed";

            destroy_swapchains();

            goto end;
        }
    }

    m_swapchain_height  = in_height;
    m_swapchain_width   = in_width;
    m_has_logged_screen = false;
    result            = true;

    /* What the images hold now is whatever was in the memory; black is shown until the application's first frame is copied. */
    {
        std::string clear_error;

        if (clear_swapchain_images(&clear_error) == false)
        {
            add_log("The images of the swap chains cannot be made black: " + clear_error);
        }
    }

    add_log("The swap chains of the eyes are " +
            std::to_string(in_width)           +
            " x "                              +
            std::to_string(in_height)          +
            ", format "                        +
            std::to_string(m_format));

end:
    return result;
}

void Headset::XR::HeadsetSession::on_session_state_changed(const XrSessionState& in_state)
{
    add_log(std::string   ("The session is ") +
            get_state_name(in_state) );

    switch (in_state)
    {
        case XR_SESSION_STATE_READY:
        {
            XrSessionBeginInfo begin_info = {XR_TYPE_SESSION_BEGIN_INFO};

            begin_info.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;

            if (XR_SUCCEEDED(::xrBeginSession(m_session,
                                             &begin_info) ))
            {
                std::string error;
                uint32_t    eye_height = 0;
                uint32_t    eye_width  = 0;

                std::lock_guard<std::mutex> lock(m_shared.mutex);

                m_shared.is_running = true;

                /* Black is shown from the start, until the frames of the application come (they make the swap chains again if they are of another
                 * size): with nothing to show, the runtime shows what it likes, and it flickers.
                 */
                if (read_recommended_eye_size(&eye_width,
                                              &eye_height) )
                {
                    if (make_swapchains(eye_width,
                                        eye_height,
                                        &error) == false)
                    {
                        add_log("The black screen cannot be shown before the first frame: " + error);
                    }
                }
            }

            break;
        }

        case XR_SESSION_STATE_STOPPING:
        {
            {
                std::lock_guard<std::mutex> lock(m_shared.mutex);

                m_shared.is_running = false;
            }

            ::xrEndSession(m_session);

            break;
        }

        case XR_SESSION_STATE_EXITING:
        case XR_SESSION_STATE_LOSS_PENDING:
        {
            m_is_exit_requested = true;

            break;
        }

        default:
        {
            break;
        }
    }
}

bool Headset::XR::HeadsetSession::pick_format(std::string* out_error_ptr)
{
    std::vector<int64_t> format_vec;
    uint32_t             n_formats = 0;
    bool                 result    = false;

    ::xrEnumerateSwapchainFormats(m_session,
                                  0,
                                 &n_formats,
                                  nullptr);

    format_vec.resize(n_formats);

    ::xrEnumerateSwapchainFormats(m_session,
                                  n_formats,
                                 &n_formats,
                                  format_vec.data() );

    /* The bytes of the images are copied as they are, so the format has to be of the family of the pixels they have; the sRGB one,
     * as they are (what is said to be sRGB is read as such by the compositor).
     */
    for (const auto& candidate_format : format_vec)
    {
        if (candidate_format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB)
        {
            m_format = candidate_format;

            break;
        }

        if (candidate_format == DXGI_FORMAT_B8G8R8A8_UNORM &&
            m_format         == 0)
        {
            m_format = candidate_format;
        }
    }

    if (m_format == 0)
    {
        *out_error_ptr = "The headset has no swap chain format with the bytes of the images (blue, green, red, alpha)";

        goto end;
    }

    result = true;

end:
    return result;
}

void Headset::XR::HeadsetSession::poll_events()
{
    XrEventDataBuffer event = {XR_TYPE_EVENT_DATA_BUFFER};

    while (::xrPollEvent(m_instance,
                        &event) == XR_SUCCESS)
    {
        if (event.type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED)
        {
            on_session_state_changed(reinterpret_cast<const XrEventDataSessionStateChanged*>(&event)->state);
        }
        else
        if (event.type == XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING)
        {
            m_is_exit_requested = true;
        }

        event = {XR_TYPE_EVENT_DATA_BUFFER};
    }
}

bool Headset::XR::HeadsetSession::probe(HeadsetInfo* out_info_ptr,
                                        std::string* out_error_ptr)
{
    bool           result = false;
    HeadsetSession session;

    if (session.create_instance(out_error_ptr) == false)
    {
        goto end;
    }

    if (session.find_system(out_error_ptr) == false)
    {
        goto end;
    }

    out_info_ptr->runtime_name = session.m_runtime_name;
    out_info_ptr->system_name  = session.m_system_name;

    /* The size of the image of an eye that the runtime recommends: what the headset is meant to be rendered at. */
    session.read_recommended_eye_size(&out_info_ptr->eye_width,
                                      &out_info_ptr->eye_height);

    result = true;

end:
    return result;
}

bool Headset::XR::HeadsetSession::read_recenter()
{
    XrActiveActionSet active_set = {};
    bool              result     = false;
    XrActionsSyncInfo sync_info  = {XR_TYPE_ACTIONS_SYNC_INFO};

    active_set.actionSet = m_action_set;

    sync_info.activeActionSets      = &active_set;
    sync_info.countActiveActionSets = 1;

    if (XR_SUCCEEDED(::xrSyncActions(m_session,
                                    &sync_info) ))
    {
        XrActionStateGetInfo get_info = {XR_TYPE_ACTION_STATE_GET_INFO};
        XrActionStateBoolean state    = {XR_TYPE_ACTION_STATE_BOOLEAN};

        get_info.action = m_action;

        if (XR_SUCCEEDED(::xrGetActionStateBoolean(m_session,
                                                  &get_info,
                                                  &state) ))
        {
            result = (state.isActive             == XR_TRUE &&
                      state.changedSinceLastSync == XR_TRUE &&
                      state.currentState         == XR_TRUE);
        }
    }

    return result;
}

bool Headset::XR::HeadsetSession::read_recommended_eye_size(uint32_t* out_width_ptr,
                                                            uint32_t* out_height_ptr) const
{
    uint32_t                n_views       = 0;
    bool                    result        = false;
    XrViewConfigurationView view_array[2] = {{XR_TYPE_VIEW_CONFIGURATION_VIEW}, {XR_TYPE_VIEW_CONFIGURATION_VIEW}};

    const XrResult enumerate_result = ::xrEnumerateViewConfigurationViews(m_instance,
                                                                          m_system_id,
                                                                          XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
                                                                          2,
                                                                         &n_views,
                                                                          view_array);

    if (XR_SUCCEEDED(enumerate_result) &&
        n_views > 0)
    {
        *out_height_ptr = view_array[0].recommendedImageRectHeight;
        *out_width_ptr  = view_array[0].recommendedImageRectWidth;
        result          = true;
    }

    return result;
}

void Headset::XR::HeadsetSession::release_eye_textures()
{
    std::lock_guard<std::mutex> lock(m_shared.mutex);

    for (auto& eye : m_shared.eye_array)
    {
        if (eye.state == SwapchainImageState::READY)
        {
            XrSwapchainImageReleaseInfo release_info = {XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};

            if (XR_SUCCEEDED(::xrReleaseSwapchainImage(eye.swapchain,
                                                      &release_info) ))
            {
                eye.has_image = true;
                eye.state     = SwapchainImageState::IDLE;
            }
        }
    }

    ++m_shared.image_serial;

    m_shared.image_condition.notify_all();
}

void Headset::XR::HeadsetSession::request_exit()
{
    m_is_exit_requested = true;

    ::xrRequestExitSession(m_session);
}

bool Headset::XR::HeadsetSession::run_frame(std::string* out_error_ptr)
{
    constexpr DWORD  IDLE_N_MILLISECONDS           = 10;
    constexpr int    IMAGE_WAIT_N_MICROSECONDS     = 6000; /* the frame has this much room before the compositor wants it */
    constexpr double LATE_PERIODS                  = 1.5;
    constexpr double REPORT_PERIOD_N_MILLISECONDS  = 1000.0;
    constexpr double N_NANOSECONDS_PER_MILLISECOND = 1000000.0;

    XrFrameState frame_state = {XR_TYPE_FRAME_STATE};
    bool         result      = true;

    (void) out_error_ptr;

    poll_events();

    if (m_is_exit_requested)
    {
        result = false;

        goto end;
    }

    {
        bool is_running = false;

        {
            std::lock_guard<std::mutex> lock(m_shared.mutex);

            is_running = m_shared.is_running;
        }

        /* A frame is waited for only in a running session: until the runtime says that it is ready, there is nothing to wait for. */
        if (is_running == false)
        {
            ::Sleep(IDLE_N_MILLISECONDS);

            goto end;
        }
    }

    {
        const double   wait_start_ms = Utils::Statistics::SeriesStats::get_now();
        const XrResult wait_result   = ::xrWaitFrame                          (m_session,
                                                                               nullptr,
                                                                              &frame_state);
        const double   wait_end_ms   = Utils::Statistics::SeriesStats::get_now();

        if (XR_SUCCEEDED(wait_result) )
        {
            auto& timing = m_timing;

            timing.period_ms = static_cast<float>(static_cast<double>(frame_state.predictedDisplayPeriod) / N_NANOSECONDS_PER_MILLISECOND);

            timing.frame_interval.set_late_threshold(LATE_PERIODS * static_cast<double>(timing.period_ms) );
            timing.frame_interval.add_interval      (wait_end_ms);
            timing.wait_cost.add                    (wait_end_ms - wait_start_ms);

            /* The compositor wants a frame: the application is to make the one that it shows next. */
            if (m_frame_func)
            {
                m_frame_func(static_cast<double>(timing.period_ms) );
            }
        }

        if (XR_FAILED(wait_result) )
        {
            log_failure("xrWaitFrame",
                        wait_result);

            ::Sleep(IDLE_N_MILLISECONDS);

            goto end;
        }
    }

    {
        XrFrameBeginInfo begin_info = {XR_TYPE_FRAME_BEGIN_INFO};

        const XrResult begin_result = ::xrBeginFrame(m_session,
                                                    &begin_info);

        if (XR_FAILED(begin_result) )
        {
            log_failure("xrBeginFrame",
                        begin_result);

            ::Sleep(IDLE_N_MILLISECONDS);

            goto end;
        }
    }

    {
        const XrCompositionLayerBaseHeader* layer_ptr_array[2] = {};
        uint32_t                            n_layers           = 0;

        /* The screen is put in front of the head the first time the head is known, and when the user says so. */
        const bool is_recenter = read_recenter();

        if (is_recenter           ||
            m_is_placed == false)
        {
            m_is_placed = locate_head(frame_state.predictedDisplayTime);

            if (is_recenter &&
                m_is_placed)
            {
                m_has_logged_screen = false;

                add_log("The screen is put where the user looks");
            }
        }

        if (frame_state.shouldRender == XR_TRUE)
        {
            auto&        timing        = m_timing;
            const double lock_start_ms = Utils::Statistics::SeriesStats::get_now();

            std::unique_lock<std::mutex> lock(m_shared.mutex);

            timing.lock_wait.add(Utils::Statistics::SeriesStats::get_now() - lock_start_ms);

            /* The images that the application's frame made are not always there when the compositor asks: a copy that is late by a few
             * milliseconds would show the frame before again, and the next one in its place a frame later. The frame is not ended for a
             * little while for them, which it has room for; if they do not come, the frame before is shown.
             */
            if (m_shared.image_serial == timing.last_serial)
            {
                const double image_wait_start_ms = Utils::Statistics::SeriesStats::get_now();

                const auto is_image_ready_func = [this, &timing]()
                                                 {
                                                     return (m_shared.image_serial != timing.last_serial ||
                                                             m_is_exit_requested);
                                                 };

                m_shared.image_condition.wait_for(lock,
                                                  std::chrono::microseconds(IMAGE_WAIT_N_MICROSECONDS),
                                                  is_image_ready_func);

                timing.image_wait.add(Utils::Statistics::SeriesStats::get_now() - image_wait_start_ms);
            }

            n_layers = make_layers(frame_state.predictedDisplayTime);

            /* Whether the images of the eyes are new to the compositor, or the ones that it showed already. */
            if (n_layers == 0)
            {
                ++timing.n_empty;
            }
            else
            if (m_shared.image_serial == timing.last_serial)
            {
                ++timing.n_repeated;
            }
            else
            {
                ++timing.n_new;

                timing.last_serial = m_shared.image_serial;
            }

            for (uint32_t n_layer = 0;
                          n_layer < n_layers;
                        ++n_layer)
            {
                layer_ptr_array[n_layer] = (m_is_cylinder_supported) ? reinterpret_cast<const XrCompositionLayerBaseHeader*>(&m_shared.cylinder_layer_array[n_layer])
                                                                     : reinterpret_cast<const XrCompositionLayerBaseHeader*>(&m_shared.quad_layer_array    [n_layer]);
            }

            const double end_start_ms = Utils::Statistics::SeriesStats::get_now();

            end_frame(frame_state.predictedDisplayTime,
                      n_layers,
                      layer_ptr_array);

            timing.end_cost.add(Utils::Statistics::SeriesStats::get_now() - end_start_ms);
        }
        else
        {
            ++m_timing.n_skipped;

            end_frame(frame_state.predictedDisplayTime,
                      0, /* in_n_layers */
                      layer_ptr_array);
        }
    }

    {
        auto&        timing = m_timing;
        const double now_ms = Utils::Statistics::SeriesStats::get_now();

        if (Utils::Statistics::SeriesStats::take_period(&timing.last_report,
                                                        now_ms,
                                                        REPORT_PERIOD_N_MILLISECONDS) )
        {
            char counts_i8vec96[96];

            ::snprintf(counts_i8vec96,
                       sizeof(counts_i8vec96),
                       "images new=%llu repeated=%llu empty=%llu skipped=%llu",
                       static_cast<unsigned long long>(timing.n_new),
                       static_cast<unsigned long long>(timing.n_repeated),
                       static_cast<unsigned long long>(timing.n_empty),
                       static_cast<unsigned long long>(timing.n_skipped) );

            timing.n_empty    = 0;
            timing.n_new      = 0;
            timing.n_repeated = 0;
            timing.n_skipped  = 0;

            add_log("Frames of the compositor (ms, the period is "        +
                    std::to_string(timing.period_ms)                      +
                    "): "                                                 +
                    timing.frame_interval.take_text("frames")             +
                    "; "                                                  +
                    timing.wait_cost.take_text    ("wait for frame")      +
                    "; "                                                  +
                    timing.lock_wait.take_text    ("wait for the lock")   +
                    "; "                                                  +
                    timing.image_wait.take_text   ("wait for new images") +
                    "; "                                                  +
                    timing.end_cost.take_text     ("end of frame")        +
                    "; "                                                  +
                    counts_i8vec96);
        }
    }

end:
    return result;
}

void Headset::XR::HeadsetSession::set_frame_func(const FrameFunc& in_frame_func)
{
    m_frame_func = in_frame_func;
}

bool Headset::XR::HeadsetSession::set_up(std::string* out_error_ptr)
{
    bool result = false;

    if (create_instance(out_error_ptr) == false)
    {
        goto end;
    }

    if (find_system(out_error_ptr) == false)
    {
        goto end;
    }

    if (create_device(out_error_ptr) == false)
    {
        goto end;
    }

    if (create_session(out_error_ptr) == false)
    {
        goto end;
    }

    if (create_spaces(out_error_ptr) == false)
    {
        goto end;
    }

    if (pick_format(out_error_ptr) == false)
    {
        goto end;
    }

    if (create_actions(out_error_ptr) == false)
    {
        goto end;
    }

    result = true;

end:
    return result;
}

std::vector<std::string> Headset::XR::HeadsetSession::take_log_lines()
{
    std::lock_guard<std::mutex> lock(m_log.mutex);

    std::vector<std::string> result_vec;

    result_vec.swap(m_log.line_vec);

    return result_vec;
}

void Headset::XR::HeadsetSession::update_field_of_view(const XrTime& in_time)
{
    XrViewLocateInfo locate_info = {XR_TYPE_VIEW_LOCATE_INFO};
    uint32_t         n_views     = 0;
    XrView           view_array[2] = {{XR_TYPE_VIEW}, {XR_TYPE_VIEW}};
    XrViewState      view_state    = {XR_TYPE_VIEW_STATE};

    locate_info.displayTime           = in_time;
    locate_info.space                 = m_local_space;
    locate_info.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;

    const XrResult locate_result = ::xrLocateViews(m_session,
                                                  &locate_info,
                                                  &view_state,
                                                   2,
                                                  &n_views,
                                                   view_array);

    if (XR_SUCCEEDED(locate_result) )
    {
        if (n_views == 2)
        {
            /* The distance between the eyes: between the places the runtime puts the two views, which it changes with the adjustment of the
             * headset. Smoothed, since it is a measure of a head that moves.
             */
            if ((view_state.viewStateFlags & XR_VIEW_STATE_POSITION_VALID_BIT) != 0)
            {
                constexpr float SMOOTHING_PER_FRAME = 0.05f;

                const float x_distance = view_array[1].pose.position.x - view_array[0].pose.position.x;
                const float y_distance = view_array[1].pose.position.y - view_array[0].pose.position.y;
                const float z_distance = view_array[1].pose.position.z - view_array[0].pose.position.z;
                const float distance   = ::sqrtf(x_distance * x_distance + y_distance * y_distance + z_distance * z_distance);
                const float previous   = m_ipd;

                if (distance > 0.0f)
                {
                    m_ipd = (previous > 0.0f) ? previous + SMOOTHING_PER_FRAME * (distance - previous)
                                            : distance;
                }
            }

            Headset::XR::FieldOfView first;
            Headset::XR::FieldOfView second;

            first.angle_down  = view_array[0].fov.angleDown;
            first.angle_left  = view_array[0].fov.angleLeft;
            first.angle_right = view_array[0].fov.angleRight;
            first.angle_up    = view_array[0].fov.angleUp;

            second.angle_down  = view_array[1].fov.angleDown;
            second.angle_left  = view_array[1].fov.angleLeft;
            second.angle_right = view_array[1].fov.angleRight;
            second.angle_up    = view_array[1].fov.angleUp;

            m_field_of_view     = ScreenGeometry::get_common_field_of_view(first,
                                                                           second);
            m_has_field_of_view = true;
        }
    }
}
