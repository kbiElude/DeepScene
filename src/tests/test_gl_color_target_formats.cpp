/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Tests for the color format of the eye render targets, which has exactly the channels of the pixel format of the window of the application.
 */
#include <gtest/gtest.h>
#include "OpenGL/gl_color_target_formats.h"
#include <windows.h>

namespace
{
    constexpr uint32_t RGB565 = 0x8D62;
    constexpr uint32_t RGB8   = 0x8051;
    constexpr uint32_t RGBA8  = 0x8058;

    Stereo::ConverterDLL::GLColorLayout make_layout(const uint32_t& in_red_bits,
                                                    const uint32_t& in_green_bits,
                                                    const uint32_t& in_blue_bits,
                                                    const uint32_t& in_alpha_bits)
    {
        Stereo::ConverterDLL::GLColorLayout result;

        result.alpha_bits = in_alpha_bits;
        result.blue_bits  = in_blue_bits;
        result.green_bits = in_green_bits;
        result.is_rgba    = true;
        result.red_bits   = in_red_bits;

        return result;
    }

    /* A window that is not shown, for a test that needs a device context. */
    HWND make_window()
    {
        return ::CreateWindowExA(0,
                                 "STATIC",
                                 "pixel format test",
                                 WS_POPUP,
                                 0,
                                 0,
                                 16,
                                 16,
                                 nullptr,
                                 nullptr,
                                 nullptr,
                                 nullptr);
    }
}

TEST(GLColorTargetFormatsTest, AWindowWithoutAlphaBitsGetsATargetWithoutAlpha)
{
    uint32_t internal_format = 0;

    EXPECT_TRUE(Stereo::ConverterDLL::GLColorTargetFormats::find_internal_format(make_layout(8, 8, 8, 0),
                                                                                &internal_format) );
    EXPECT_EQ  (internal_format,
                RGB8);
}

TEST(GLColorTargetFormatsTest, AWindowWithEightAlphaBitsGetsATargetWithAlpha)
{
    uint32_t internal_format = 0;

    EXPECT_TRUE(Stereo::ConverterDLL::GLColorTargetFormats::find_internal_format(make_layout(8, 8, 8, 8),
                                                                                &internal_format) );
    EXPECT_EQ  (internal_format,
                RGBA8);
}

TEST(GLColorTargetFormatsTest, ASixteenBitWindowGetsFiveSixFive)
{
    uint32_t internal_format = 0;

    EXPECT_TRUE(Stereo::ConverterDLL::GLColorTargetFormats::find_internal_format(make_layout(5, 6, 5, 0),
                                                                                &internal_format) );
    EXPECT_EQ  (internal_format,
                RGB565);
}

TEST(GLColorTargetFormatsTest, ALayoutWithoutATargetOfExactlyItsChannelsHasNoFormat)
{
    uint32_t internal_format = 1234;

    EXPECT_FALSE(Stereo::ConverterDLL::GLColorTargetFormats::find_internal_format(make_layout(10, 10, 10, 2),
                                                                                 &internal_format) );
    EXPECT_FALSE(Stereo::ConverterDLL::GLColorTargetFormats::find_internal_format(make_layout(8, 8, 8, 1),
                                                                                 &internal_format) );
    EXPECT_FALSE(Stereo::ConverterDLL::GLColorTargetFormats::find_internal_format(make_layout(8, 8, 0, 0),
                                                                                 &internal_format) );
    EXPECT_EQ   (internal_format,
                 1234u) << "nothing is written when there is no format";
}

TEST(GLColorTargetFormatsTest, AColorIndexPixelFormatHasNoFormat)
{
    Stereo::ConverterDLL::GLColorLayout layout          = make_layout(8, 8, 8, 8);
    uint32_t                            internal_format = 0;

    layout.is_rgba = false;

    EXPECT_FALSE(Stereo::ConverterDLL::GLColorTargetFormats::find_internal_format(layout,
                                                                                 &internal_format) );
}

TEST(GLColorTargetFormatsTest, TheLayoutReadIsTheOneOfThePixelFormatSetOnTheDevice)
{
    PIXELFORMATDESCRIPTOR request = {};

    request.cColorBits = 32;
    request.cDepthBits = 24;
    request.dwFlags    = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    request.iPixelType = PFD_TYPE_RGBA;
    request.nSize      = sizeof(request);
    request.nVersion   = 1;

    const HWND window   = make_window();
    const HDC  device   = (window != nullptr) ? ::GetDC(window)
                                              : nullptr;
    const int  n_format = (device != nullptr) ? ::ChoosePixelFormat(device,
                                                                   &request)
                                              : 0;
    bool       is_set   = false;

    if (n_format != 0)
    {
        is_set = (::SetPixelFormat(device,
                                   n_format,
                                  &request) != FALSE);
    }

    if (is_set == false)
    {
        if (window != nullptr)
        {
            ::DestroyWindow(window);
        }

        GTEST_SKIP() << "no pixel format can be set on a window here";
    }

    Stereo::ConverterDLL::GLColorLayout layout;
    PIXELFORMATDESCRIPTOR               described = {};

    ::DescribePixelFormat(device,
                          n_format,
                          sizeof(described),
                         &described);

    EXPECT_TRUE(Stereo::ConverterDLL::GLColorTargetFormats::read_layout(device,
                                                                       &layout) );
    EXPECT_EQ  (layout.alpha_bits,
                static_cast<uint32_t>(described.cAlphaBits) );
    EXPECT_EQ  (layout.blue_bits,
                static_cast<uint32_t>(described.cBlueBits) );
    EXPECT_EQ  (layout.green_bits,
                static_cast<uint32_t>(described.cGreenBits) );
    EXPECT_EQ  (layout.red_bits,
                static_cast<uint32_t>(described.cRedBits) );
    EXPECT_TRUE(layout.is_rgba);

    ::ReleaseDC(window,
                device);
    ::DestroyWindow(window);
}

TEST(GLColorTargetFormatsTest, ADeviceWithoutAPixelFormatHasNoLayout)
{
    const HWND window = make_window();
    const HDC  device = (window != nullptr) ? ::GetDC(window)
                                            : nullptr;

    if (device == nullptr)
    {
        GTEST_SKIP() << "no window here";
    }

    Stereo::ConverterDLL::GLColorLayout layout;

    EXPECT_FALSE(Stereo::ConverterDLL::GLColorTargetFormats::read_layout(device,
                                                                        &layout) );

    ::ReleaseDC(window,
                device);
    ::DestroyWindow(window);
}
