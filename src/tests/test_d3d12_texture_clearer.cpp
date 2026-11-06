/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Tests for filling textures of a device with black, on the GPU of the machine. A machine with no GPU which does Direct3D 12 skips them.
 */
#include <gtest/gtest.h>
#include "d3d12_device_fixture.h"
#include "headset/d3d12/d3d12_texture_clearer.h"

class D3D12TextureClearerTest : public D3D12DeviceFixture
{
protected:
    /* What a texture of an eye of black pixels holds: blue, green and red 0, and alpha 255. */
    static std::vector<uint8_t> make_black_expected()
    {
        std::vector<uint8_t> result_vec(static_cast<size_t>(EYE_WIDTH) * EYE_HEIGHT * 4);

        for (size_t n_byte = 3;
                    n_byte < result_vec.size();
                    n_byte += 4)
        {
            result_vec.at(n_byte) = 255;
        }

        return result_vec;
    }
};

TEST_F(D3D12TextureClearerTest, ATextureIsBlackAndOpaqueAndIsInTheStateItWasIn)
{
    std::string error;

    Headset::D3D12::D3D12TextureClearer clearer(m_device_ptr.Get(),
                                               m_queue_ptr.Get() );
    auto                               texture_ptr = create_texture(DXGI_FORMAT_B8G8R8A8_UNORM,
                                                                    D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
                                                                    D3D12_RESOURCE_STATE_COPY_SOURCE);

    ASSERT_TRUE(clearer.is_ok(&error) ) << error;
    ASSERT_NE(texture_ptr,
              nullptr);

    /* A state other than the one of a render target is moved to it and back. */
    ASSERT_TRUE(clearer.clear_to_black(texture_ptr.Get(),
                                       DXGI_FORMAT_B8G8R8A8_UNORM,
                                       D3D12_RESOURCE_STATE_COPY_SOURCE,
                                      &error) ) << error;

    EXPECT_EQ(read_back(texture_ptr.Get() ),
              make_black_expected() );
}

TEST_F(D3D12TextureClearerTest, ATextureInTheStateOfARenderTargetIsClearedAgainAndAgain)
{
    std::string error;

    Headset::D3D12::D3D12TextureClearer clearer(m_device_ptr.Get(),
                                               m_queue_ptr.Get() );
    auto                               left_ptr  = create_texture(DXGI_FORMAT_B8G8R8A8_UNORM_SRGB,
                                                                  D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
                                                                  D3D12_RESOURCE_STATE_RENDER_TARGET);
    auto                               right_ptr = create_texture(DXGI_FORMAT_B8G8R8A8_UNORM_SRGB,
                                                                  D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
                                                                  D3D12_RESOURCE_STATE_RENDER_TARGET);

    ASSERT_NE(left_ptr,
              nullptr);
    ASSERT_NE(right_ptr,
              nullptr);

    /* One clearer does a texture after another, and a texture again. */
    for (ID3D12Resource* texture_ptr : {left_ptr.Get(), right_ptr.Get(), left_ptr.Get()})
    {
        ASSERT_TRUE(clearer.clear_to_black(texture_ptr,
                                           DXGI_FORMAT_B8G8R8A8_UNORM_SRGB,
                                           D3D12_RESOURCE_STATE_RENDER_TARGET,
                                          &error) ) << error;
    }

    EXPECT_EQ(read_back(left_ptr.Get(),
                        D3D12_RESOURCE_STATE_RENDER_TARGET),
              make_black_expected() );
    EXPECT_EQ(read_back(right_ptr.Get(),
                        D3D12_RESOURCE_STATE_RENDER_TARGET),
              make_black_expected() );
}
