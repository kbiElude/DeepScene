/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef VRDEVICES_I_VR_DEVICE_PROVIDER_H
#define VRDEVICES_I_VR_DEVICE_PROVIDER_H

#include "VR/VRDevices/types.h"
#include <string>
#include <vector>

namespace VR
{
    namespace VRDevices
    {
        /* Where the list of devices comes from: made-up devices for now, the OpenXR runtimes of the machine later. */
        class IVrDeviceProvider
        {
        public:
            virtual ~IVrDeviceProvider() = default;

            /* The devices, in the order they are shown, each with an id of its own. Returns false, and says why, if they cannot be
             * listed.
             */
            virtual bool enumerate(std::vector<VrDevice>* out_device_vec_ptr,
                                   std::string*           out_error_ptr) const = 0;
        };
    }
}

#endif /* VRDEVICES_I_VR_DEVICE_PROVIDER_H */
