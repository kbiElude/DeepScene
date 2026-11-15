/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef HEADSET_VRDEVICES_STUB_VR_DEVICE_PROVIDER_H
#define HEADSET_VRDEVICES_STUB_VR_DEVICE_PROVIDER_H

#include "headset/vr_devices/i_vr_device_provider.h"

namespace Headset
{
    namespace VRDevices
    {
        /* Simulated devices, always the same: two that can be used and one that is not connected, so that the list shows both cases. */
        class StubVrDeviceProvider : public IVrDeviceProvider
        {
        public:
            bool enumerate(std::vector<VrDevice>* out_device_vec_ptr,
                           std::string*           out_error_ptr) const override;
        };
    }
}

#endif /* HEADSET_VRDEVICES_STUB_VR_DEVICE_PROVIDER_H */
