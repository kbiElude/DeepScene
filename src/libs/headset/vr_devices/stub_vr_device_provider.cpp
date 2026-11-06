/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "headset/vr_devices/stub_vr_device_provider.h"

bool Headset::VRDevices::StubVrDeviceProvider::enumerate(std::vector<VrDevice>* out_device_vec_ptr,
                                                    std::string*           out_error_ptr) const
{
    VrDevice device;

    (void) out_error_ptr;

    out_device_vec_ptr->clear();

    device.runtime_name = "Simulated runtime";

    device.id   = "stub-headset";
    device.name = "Simulated headset";

    out_device_vec_ptr->push_back(device);

    device.id   = "stub-headset-wide";
    device.name = "Simulated headset, wide field of view";

    out_device_vec_ptr->push_back(device);

    device.id                 = "stub-headset-offline";
    device.name               = "Simulated headset, not connected";
    device.unavailable_reason = "The headset is not connected.";

    out_device_vec_ptr->push_back(device);

    return true;
}
