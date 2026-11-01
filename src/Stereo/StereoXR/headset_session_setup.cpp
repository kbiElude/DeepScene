/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "headset_session_impl.h"
#include <string.h>

bool Stereo::StereoXR::HeadsetSession::Impl::create_actions(std::string* out_error_ptr)
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

    if (XR_FAILED(::xrCreateActionSet(instance,
                                     &action_set_info,
                                     &action_set) ))
    {
        *out_error_ptr = "The action to put the screen where the user looks cannot be made";

        goto end;
    }

    if (XR_FAILED(::xrCreateAction(action_set,
                                  &action_info,
                                  &action) ))
    {
        *out_error_ptr = "The action to put the screen where the user looks cannot be made";

        goto end;
    }

    for (const auto& profile_binding : profile_binding_array)
    {
        XrActionSuggestedBinding             binding_array[2]  = {};
        XrInteractionProfileSuggestedBinding suggested_binding = {XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};

        binding_array[0].action = action;
        binding_array[1].action = action;

        if (XR_FAILED(::xrStringToPath(instance,
                                       profile_binding.path_a_ptr,
                                      &binding_array[0].binding) ))
        {
            continue;
        }

        if (XR_FAILED(::xrStringToPath(instance,
                                       profile_binding.path_b_ptr,
                                      &binding_array[1].binding) ))
        {
            continue;
        }

        if (XR_FAILED(::xrStringToPath(instance,
                                       profile_binding.interaction_profile_ptr,
                                      &suggested_binding.interactionProfile) ))
        {
            continue;
        }

        suggested_binding.countSuggestedBindings = 2;
        suggested_binding.suggestedBindings      = binding_array;

        /* Not an error: what a runtime does not know of is not used. */
        ::xrSuggestInteractionProfileBindings(instance,
                                             &suggested_binding);
    }

    attach_info.actionSets      = &action_set;
    attach_info.countActionSets = 1;

    if (XR_FAILED(::xrAttachSessionActionSets(session,
                                             &attach_info) ))
    {
        *out_error_ptr = "The action cannot be given to the session";

        goto end;
    }

    result = true;

end:
    return result;
}

bool Stereo::StereoXR::HeadsetSession::Impl::create_device(std::string* out_error_ptr)
{
    Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter_ptr;
    Microsoft::WRL::ComPtr<IDXGIFactory4> factory_ptr;
    PFN_xrGetD3D12GraphicsRequirementsKHR get_requirements_func_ptr = nullptr;
    D3D12_COMMAND_QUEUE_DESC              queue_desc                = {};
    XrGraphicsRequirementsD3D12KHR        requirements              = {XR_TYPE_GRAPHICS_REQUIREMENTS_D3D12_KHR};
    bool                                  result                    = false;

    if (XR_FAILED(::xrGetInstanceProcAddr(instance,
                                          "xrGetD3D12GraphicsRequirementsKHR",
                                          reinterpret_cast<PFN_xrVoidFunction*>(&get_requirements_func_ptr) )))
    {
        *out_error_ptr = "The headset does not say which adapter it is on";

        goto end;
    }

    if (XR_FAILED(get_requirements_func_ptr(instance,
                                            system_id,
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
                                   IID_PPV_ARGS(device_ptr.GetAddressOf() ))))
    {
        *out_error_ptr = "The device for the adapter that the headset is on cannot be made";

        goto end;
    }

    queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

    if (FAILED(device_ptr->CreateCommandQueue(&queue_desc,
                                              IID_PPV_ARGS(queue_ptr.GetAddressOf() ))))
    {
        *out_error_ptr = "The command queue for the headset cannot be made";

        goto end;
    }

    result = true;

end:
    return result;
}

bool Stereo::StereoXR::HeadsetSession::Impl::create_instance(std::string* out_error_ptr)
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
                                                                       "(for a Quest, set Meta Quest Link as the active OpenXR runtime in its settings; result "                +
                                                                       std::to_string(static_cast<int>(xr_result))                                                              +
                                                                       ")"
                                                                     : "There is no OpenXR runtime (its extensions cannot be listed; result "                                   +
                                                                       std::to_string(static_cast<int>(xr_result))                                                              +
                                                                       ")";

        goto end;
    }

    for (const auto& extension : extension_vec)
    {
        has_d3d12             = has_d3d12             || (::strcmp(extension.extensionName,
                                                                   XR_KHR_D3D12_ENABLE_EXTENSION_NAME)               == 0);
        is_cylinder_supported = is_cylinder_supported || (::strcmp(extension.extensionName,
                                                                   XR_KHR_COMPOSITION_LAYER_CYLINDER_EXTENSION_NAME) == 0);
    }

    if (has_d3d12 == false)
    {
        *out_error_ptr = "The OpenXR runtime does not take textures of Direct3D 12 (it has no " XR_KHR_D3D12_ENABLE_EXTENSION_NAME ")";

        goto end;
    }

    if (is_cylinder_supported)
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
                                       &instance);
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

        if (XR_SUCCEEDED(::xrGetInstanceProperties(instance,
                                                  &instance_properties) ))
        {
            runtime_name = instance_properties.runtimeName;
        }
    }

    result = true;

end:
    return result;
}

bool Stereo::StereoXR::HeadsetSession::Impl::create_session(std::string* out_error_ptr)
{
    XrGraphicsBindingD3D12KHR binding      = {XR_TYPE_GRAPHICS_BINDING_D3D12_KHR};
    bool                      result       = false;
    XrSessionCreateInfo       session_info = {XR_TYPE_SESSION_CREATE_INFO};
    XrResult                  xr_result    = XR_SUCCESS;

    binding.device = device_ptr.Get();
    binding.queue  = queue_ptr.Get();

    session_info.next     = &binding;
    session_info.systemId = system_id;

    xr_result = ::xrCreateSession(instance,
                                 &session_info,
                                 &session);

    if (XR_FAILED(xr_result) )
    {
        *out_error_ptr = "The session of the headset cannot be made (" + get_result_text(xr_result) + ")";

        goto end;
    }

    result = true;

end:
    return result;
}

bool Stereo::StereoXR::HeadsetSession::Impl::create_spaces(std::string* out_error_ptr)
{
    bool                       result     = false;
    XrReferenceSpaceCreateInfo space_info = {XR_TYPE_REFERENCE_SPACE_CREATE_INFO};

    space_info.poseInReferenceSpace.orientation.w = 1.0f;
    space_info.referenceSpaceType                 = XR_REFERENCE_SPACE_TYPE_LOCAL;

    if (XR_FAILED(::xrCreateReferenceSpace(session,
                                          &space_info,
                                          &local_space) ))
    {
        *out_error_ptr = "The space of the room cannot be made";

        goto end;
    }

    space_info.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;

    if (XR_FAILED(::xrCreateReferenceSpace(session,
                                          &space_info,
                                          &view_space) ))
    {
        *out_error_ptr = "The space of the head cannot be made";

        goto end;
    }

    result = true;

end:
    return result;
}

bool Stereo::StereoXR::HeadsetSession::Impl::find_system(std::string* out_error_ptr)
{
    bool            result      = false;
    XrSystemGetInfo system_info = {XR_TYPE_SYSTEM_GET_INFO};
    XrResult        xr_result   = XR_SUCCESS;

    system_info.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;

    xr_result = ::xrGetSystem(instance,
                             &system_info,
                             &system_id);

    if (XR_FAILED(xr_result) )
    {
        *out_error_ptr = "There is no headset ("    +
                         get_result_text(xr_result) +
                         ")";

        goto end;
    }

    {
        XrSystemProperties system_properties = {XR_TYPE_SYSTEM_PROPERTIES};

        if (XR_FAILED(::xrGetSystemProperties(instance,
                                              system_id,
                                             &system_properties) ))
        {
            *out_error_ptr = "The properties of the headset cannot be read";

            goto end;
        }

        max_height  = system_properties.graphicsProperties.maxSwapchainImageHeight;
        max_width   = system_properties.graphicsProperties.maxSwapchainImageWidth;
        system_name = system_properties.systemName;
    }

    result = true;

end:
    return result;
}

bool Stereo::StereoXR::HeadsetSession::Impl::pick_format(std::string* out_error_ptr)
{
    std::vector<int64_t> format_vec;
    uint32_t             n_formats = 0;
    bool                 result    = false;

    ::xrEnumerateSwapchainFormats(session,
                                  0,
                                 &n_formats,
                                  nullptr);

    format_vec.resize(n_formats);

    ::xrEnumerateSwapchainFormats(session,
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
            format = candidate_format;

            break;
        }

        if (candidate_format == DXGI_FORMAT_B8G8R8A8_UNORM &&
            format           == 0)
        {
            format = candidate_format;
        }
    }

    if (format == 0)
    {
        *out_error_ptr = "The headset has no swap chain format with the bytes of the images (blue, green, red, alpha)";

        goto end;
    }

    result = true;

end:
    return result;
}

bool Stereo::StereoXR::HeadsetSession::Impl::set_up(std::string* out_error_ptr)
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
