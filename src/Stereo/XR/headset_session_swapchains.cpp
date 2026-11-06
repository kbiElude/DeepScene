/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "headset_session_impl.h"

bool Stereo::XR::HeadsetSession::acquire_eye_textures(const uint32_t&  in_width,
                                                      const uint32_t&  in_height,
                                                      ID3D12Resource** out_left_texture_ptr_ptr,
                                                      ID3D12Resource** out_right_texture_ptr_ptr,
                                                      std::string*     out_error_ptr)
{
    constexpr int64_t WAIT_N_NANOSECONDS = 8 * 1000 * 1000; /* the compositor gives an image back within a frame, which is 14 ms at 72 Hz */

    bool result = false;

    {
        std::lock_guard<std::mutex> lock(m_impl_ptr->mutex);

        if (m_impl_ptr->is_running == false)
        {
            *out_error_ptr = "The session of the headset is not running";

            goto end;
        }

        if (in_width  > m_impl_ptr->max_width ||
            in_height > m_impl_ptr->max_height)
        {
            *out_error_ptr = "An image of an eye ("                 +
                             std::to_string(in_width)               +
                             " x "                                  +
                             std::to_string(in_height)              +
                             ") is larger than the headset takes (" +
                             std::to_string(m_impl_ptr->max_width)  +
                             " x "                                  +
                             std::to_string(m_impl_ptr->max_height) +
                             ")";

            goto end;
        }

        if (m_impl_ptr->make_swapchains(in_width,
                                        in_height,
                                        out_error_ptr) == false)
        {
            goto end;
        }

        for (auto& eye : m_impl_ptr->eye_array)
        {
            XrResult wait_result = XR_SUCCESS;

            if (eye.state == Impl::ImageState::IDLE)
            {
                XrSwapchainImageAcquireInfo acquire_info = {XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};

                if (XR_FAILED(::xrAcquireSwapchainImage(eye.swapchain,
                                                       &acquire_info,
                                                       &eye.acquired_index) ))
                {
                    *out_error_ptr = "An image of the swap chain cannot be taken";

                    goto end;
                }

                eye.state = Impl::ImageState::ACQUIRED;
            }

            if (eye.state == Impl::ImageState::ACQUIRED)
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

                eye.state = Impl::ImageState::READY;
            }
        }

        *out_left_texture_ptr_ptr  = m_impl_ptr->eye_array.at(0).image_vec.at(m_impl_ptr->eye_array.at(0).acquired_index).texture;
        *out_right_texture_ptr_ptr = m_impl_ptr->eye_array.at(1).image_vec.at(m_impl_ptr->eye_array.at(1).acquired_index).texture;

        result = true;
    }

end:
    return result;
}

bool Stereo::XR::HeadsetSession::Impl::clear_swapchain_images(std::string* out_error_ptr)
{
    constexpr int64_t WAIT_N_NANOSECONDS = 100 * 1000 * 1000; /* a swap chain that was just made gives its images back at once */

    const D3D12_RESOURCE_STATES texture_state = HeadsetSession::get_texture_state();
    const DXGI_FORMAT           view_format   = static_cast<DXGI_FORMAT>(format);
    bool                        result        = true;

    if (texture_clearer_ptr == nullptr)
    {
        texture_clearer_ptr.reset(new Stereo::D3D12::D3D12TextureClearer(device_ptr.Get(),
                                                                         queue_ptr.Get() ));
    }

    if (texture_clearer_ptr->is_ok(out_error_ptr) == false)
    {
        result = false;

        goto end;
    }

    for (auto& eye : eye_array)
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

                if (texture_clearer_ptr->clear_to_black(texture_ptr,
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

void Stereo::XR::HeadsetSession::Impl::destroy_swapchains()
{
    for (auto& eye : eye_array)
    {
        if (eye.swapchain != XR_NULL_HANDLE)
        {
            ::xrDestroySwapchain(eye.swapchain);
        }

        eye = EyeSwapchain();
    }

    swapchain_height = 0;
    swapchain_width  = 0;
}

bool Stereo::XR::HeadsetSession::Impl::make_swapchains(const uint32_t& in_width,
                                                       const uint32_t& in_height,
                                                       std::string*    out_error_ptr)
{
    bool result = false;

    if (swapchain_width  == in_width  &&
        swapchain_height == in_height &&
        eye_array.at(0).swapchain != XR_NULL_HANDLE)
    {
        result = true;

        goto end;
    }

    destroy_swapchains();

    for (auto& eye : eye_array)
    {
        XrSwapchainCreateInfo swapchain_info = {XR_TYPE_SWAPCHAIN_CREATE_INFO};
        uint32_t              n_images       = 0;

        swapchain_info.arraySize   = 1;
        swapchain_info.faceCount   = 1;
        swapchain_info.format      = format;
        swapchain_info.height      = in_height;
        swapchain_info.mipCount    = 1;
        swapchain_info.sampleCount = 1;
        swapchain_info.usageFlags  = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT;
        swapchain_info.width       = in_width;

        if (XR_FAILED(::xrCreateSwapchain(session,
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

    swapchain_height  = in_height;
    swapchain_width   = in_width;
    has_logged_screen = false;
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
            std::to_string(format));

end:
    return result;
}

void Stereo::XR::HeadsetSession::release_eye_textures()
{
    std::lock_guard<std::mutex> lock(m_impl_ptr->mutex);

    for (auto& eye : m_impl_ptr->eye_array)
    {
        if (eye.state == Impl::ImageState::READY)
        {
            XrSwapchainImageReleaseInfo release_info = {XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};

            if (XR_SUCCEEDED(::xrReleaseSwapchainImage(eye.swapchain,
                                                      &release_info) ))
            {
                eye.has_image = true;
                eye.state     = Impl::ImageState::IDLE;
            }
        }
    }

    ++m_impl_ptr->image_serial;

    m_impl_ptr->image_condition.notify_all();
}
