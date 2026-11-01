/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "Stereo/StereoXR/openxr_vr_device_provider.h"
#include "Stereo/StereoXR/headset_session.h"

bool Stereo::StereoXR::OpenXrVrDeviceProvider::enumerate(std::vector<VR::VRDevices::VrDevice>* out_device_vec_ptr,
                                                         std::string*                          out_error_ptr) const
{
    HeadsetInfo info;
    std::string reason;

    (void) out_error_ptr;

    out_device_vec_ptr->clear();

    if (HeadsetSession::probe(&info,
                              &reason) )
    {
        VR::VRDevices::VrDevice device;

        device.eye_height   = info.eye_height;
        device.eye_width    = info.eye_width;
        device.form_factor  = VR::VRDevices::FormFactor::HEAD_MOUNTED_DISPLAY;
        device.id           = "openxr:"         +
                              info.runtime_name +
                              ":"               +
                              info.system_name;
        device.name         = info.system_name;
        device.runtime_name = info.runtime_name;

        out_device_vec_ptr->push_back(device);
    }

    return true;
}
