/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_XR_OPENXR_VR_DEVICE_PROVIDER_H
#define STEREO_XR_OPENXR_VR_DEVICE_PROVIDER_H

#include "VR/VRDevices/i_vr_device_provider.h"

namespace Stereo
{
    namespace XR
    {
        /* The headset of the OpenXR runtime which is active on the machine, if it is there and connected: one device. With no runtime, or no
         * headset, there are no devices, and that is not an error: the picture is shown on the screen only.
         */
        class OpenXrVrDeviceProvider : public VR::VRDevices::IVrDeviceProvider
        {
        public:
            bool enumerate(std::vector<VR::VRDevices::VrDevice>* out_device_vec_ptr,
                           std::string*                          out_error_ptr) const override;
        };
    }
}

#endif /* STEREO_XR_OPENXR_VR_DEVICE_PROVIDER_H */
