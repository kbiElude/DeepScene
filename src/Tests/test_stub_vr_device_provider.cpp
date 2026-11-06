/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Tests for the stub provider of VR devices: the list is always the same, every device has an id of its own, and one device is
 * there which cannot be used, with the reason.
 */
#include <gtest/gtest.h>
#include "Headset/VRDevices/stub_vr_device_provider.h"
#include <set>

TEST(StubVrDeviceProviderTest, TheListIsTheSameEveryTime)
{
    const Headset::VRDevices::StubVrDeviceProvider provider;
    std::string                               error;
    std::vector<Headset::VRDevices::VrDevice>      first_vec;
    std::vector<Headset::VRDevices::VrDevice>      second_vec;

    ASSERT_TRUE(provider.enumerate(&first_vec,
                                   &error) ) << error;
    ASSERT_TRUE(provider.enumerate(&second_vec,
                                   &error) ) << error;
    ASSERT_EQ  (first_vec.size(),
                second_vec.size() );

    for (size_t n_device = 0;
                n_device < first_vec.size();
              ++n_device)
    {
        EXPECT_EQ(first_vec.at(n_device).id,
                  second_vec.at(n_device).id);
        EXPECT_EQ(first_vec.at(n_device).name,
                  second_vec.at(n_device).name);
    }
}

TEST(StubVrDeviceProviderTest, EveryDeviceHasAnIdAndANameAndAnIdOfItsOwn)
{
    const Headset::VRDevices::StubVrDeviceProvider provider;
    std::set<std::string>                     id_set;
    std::string                               error;
    std::vector<Headset::VRDevices::VrDevice>      device_vec;

    ASSERT_TRUE(provider.enumerate(&device_vec,
                                   &error) ) << error;
    ASSERT_FALSE(device_vec.empty() );

    for (const auto& device : device_vec)
    {
        EXPECT_FALSE(device.id.empty() );
        EXPECT_FALSE(device.name.empty() );
        EXPECT_FALSE(device.runtime_name.empty() );
        EXPECT_TRUE (id_set.insert(device.id).second) << device.id;
    }
}

TEST(StubVrDeviceProviderTest, SomeDevicesCanBeUsedAndOneCannotWithTheReason)
{
    const Headset::VRDevices::StubVrDeviceProvider provider;
    size_t                                    n_unavailable = 0;
    size_t                                    n_usable      = 0;
    std::string                               error;
    std::vector<Headset::VRDevices::VrDevice>      device_vec;

    ASSERT_TRUE(provider.enumerate(&device_vec,
                                   &error) ) << error;

    for (const auto& device : device_vec)
    {
        if (device.unavailable_reason.empty() )
        {
            ++n_usable;
        }
        else
        {
            ++n_unavailable;
        }
    }

    EXPECT_GE(n_usable,
              2u);
    EXPECT_EQ(n_unavailable,
              1u);
}

TEST(StubVrDeviceProviderTest, WhatWasInTheVectorIsReplacedNotAddedTo)
{
    const Headset::VRDevices::StubVrDeviceProvider stub;
    const Headset::VRDevices::IVrDeviceProvider&   provider   = stub;
    std::vector<Headset::VRDevices::VrDevice>      device_vec = {Headset::VRDevices::VrDevice()};

    ASSERT_TRUE(provider.enumerate(&device_vec,
                                   nullptr) );

    EXPECT_EQ(device_vec.size(),
              3u);
}
