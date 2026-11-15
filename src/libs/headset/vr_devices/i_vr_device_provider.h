/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef HEADSET_VRDEVICES_I_VR_DEVICE_PROVIDER_H
#define HEADSET_VRDEVICES_I_VR_DEVICE_PROVIDER_H

#include "headset/vr_devices/types.h"
#include <string>
#include <vector>

namespace Headset
{
    namespace VRDevices
    {
        /* A source of the device list: the OpenXR runtime of the machine (OpenXrVrDeviceProvider) or simulated devices
         * (StubVrDeviceProvider).
         */
        class IVrDeviceProvider
        {
        public:
            virtual ~IVrDeviceProvider() = default;

            /* Lists the devices in the order they are shown, each with its own id. Returns false and sets out_error_ptr to the reason if
             * they cannot be listed.
             */
            virtual bool enumerate(std::vector<VrDevice>* out_device_vec_ptr,
                                   std::string*           out_error_ptr) const = 0;
        };
    }
}

#endif /* HEADSET_VRDEVICES_I_VR_DEVICE_PROVIDER_H */
