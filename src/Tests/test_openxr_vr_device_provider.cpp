/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Tests for the list of VR devices which comes from the OpenXR runtime of the machine. Whether the machine has a runtime and a headset is
 * not known to the test, so it checks what holds either way.
 */
#include <gtest/gtest.h>
#include "Headset/XR/headset_session.h"
#include "Headset/XR/openxr_vr_device_provider.h"

TEST(OpenXrVrDeviceProviderTest, WithNoRuntimeOrNoHeadsetThereAreNoDevicesAndThatIsNotAnError)
{
    const Headset::XR::OpenXrVrDeviceProvider provider;
    std::vector<Headset::VRDevices::VrDevice>     device_vec = {Headset::VRDevices::VrDevice()};
    std::string                              error;

    ASSERT_TRUE(provider.enumerate(&device_vec,
                                   &error) ) << error;

    /* A list from before is not kept; a device which is there has a name, the runtime that serves it, and can be played on. */
    EXPECT_LE(device_vec.size(),
              1u);

    for (const auto& device : device_vec)
    {
        EXPECT_FALSE(device.name.empty() );
        EXPECT_FALSE(device.id.empty() );
        EXPECT_TRUE (device.unavailable_reason.empty() );
        EXPECT_EQ   (device.form_factor,
                     Headset::VRDevices::FormFactor::HEAD_MOUNTED_DISPLAY);
    }
}

TEST(OpenXrVrDeviceProviderTest, AProbeOfTheHeadsetSaysWhyThereIsNoneOrWhatItIs)
{
    Headset::XR::HeadsetInfo info;
    std::string             error;

    if (Headset::XR::HeadsetSession::probe(&info,
                                          &error) )
    {
        EXPECT_FALSE(info.system_name.empty() );
    }
    else
    {
        EXPECT_FALSE(error.empty() );
    }
}
