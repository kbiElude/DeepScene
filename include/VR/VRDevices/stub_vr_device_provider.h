/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef VRDEVICES_STUB_VR_DEVICE_PROVIDER_H
#define VRDEVICES_STUB_VR_DEVICE_PROVIDER_H

#include "VR/VRDevices/i_vr_device_provider.h"

namespace VR
{
    namespace VRDevices
    {
        /* Made-up devices, always the same: two which can be used, and one which is not connected, so that the list shows both. */
        class StubVrDeviceProvider : public IVrDeviceProvider
        {
        public:
            bool enumerate(std::vector<VrDevice>* out_device_vec_ptr,
                           std::string*           out_error_ptr) const override;
        };
    }
}

#endif /* VRDEVICES_STUB_VR_DEVICE_PROVIDER_H */
