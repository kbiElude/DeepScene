/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Checks of the OpenGL front end taking the size of the viewport from the window when the program never calls glViewport: the viewport of a
 * context starts as the client area of the window that it is first made current to.
 */
#include <gtest/gtest.h>
#include "gl_frontend_fixture.h"

namespace
{
    class GLFrontendWindows : public Tests::GLFrontendFixture
    {
    protected:
        enum : uint64_t
        {
            WINDOW_ADDRESS = 0x2102d0
        };

        /* WS_POPUP | WS_VISIBLE: no frame, so the client area is the rectangle of the window. */
        static constexpr uint32_t POPUP_STYLE            = 0x90000000;
        static constexpr uint32_t USE_DEFAULT_SIZE_VALUE = 0x80000000;

        /* A window, its device context, and a context made current to it. */
        void make_window_and_context(const int32_t& in_width,
                                     const int32_t& in_height)
        {
            m_builder.create_window(WINDOW_ADDRESS,
                                    POPUP_STYLE,
                                    0, /* in_ex_style */
                                    in_width,
                                    in_height);
            m_builder.get_dc       (WINDOW_ADDRESS,
                                    DEVICE_CONTEXT_ADDRESS);
            m_builder.make_current (DEVICE_CONTEXT_ADDRESS,
                                    CONTEXT_ADDRESS);
        }
    };
}

TEST_F(GLFrontendWindows, AProgramThatNeverSetsTheViewportDrawsToTheWholeWindow)
{
    make_window_and_context(1280,
                            720);

    m_builder.clear       (COLOR_BUFFER_BIT | DEPTH_BUFFER_BIT);
    m_builder.swap_buffers(DEVICE_CONTEXT_ADDRESS);

    feed();

    const auto& model = m_harness.get_model();

    ASSERT_EQ(model.pass_structure_interner.get_n_structures(),
              1u);
    EXPECT_EQ(model.pass_structure_interner.get_structure(0).viewport.width,
              1280.0f);
    EXPECT_EQ(model.pass_structure_interner.get_structure(0).viewport.height,
              720.0f);

    ASSERT_EQ(model.resource_vec.size(),
              2u);
    EXPECT_EQ(model.resource_vec.at(0).description.width,
              1280u);
    EXPECT_EQ(model.resource_vec.at(0).description.height,
              720u);
}

TEST_F(GLFrontendWindows, AViewportThatTheProgramSetsIsUsedInsteadOfTheWindow)
{
    make_window_and_context(1280,
                            720);

    m_builder.viewport    (0,
                           0,
                           640,
                           360);
    m_builder.clear       (COLOR_BUFFER_BIT);
    m_builder.swap_buffers(DEVICE_CONTEXT_ADDRESS);

    feed();

    EXPECT_EQ(m_harness.get_model().pass_structure_interner.get_structure(0).viewport.width,
              640.0f);
}

TEST_F(GLFrontendWindows, AWindowOfTheDefaultSizeHasNoSizeToTake)
{
    make_window_and_context(static_cast<int32_t>(USE_DEFAULT_SIZE_VALUE),
                            static_cast<int32_t>(USE_DEFAULT_SIZE_VALUE) );

    m_builder.clear       (COLOR_BUFFER_BIT);
    m_builder.swap_buffers(DEVICE_CONTEXT_ADDRESS);

    feed();

    EXPECT_EQ(m_harness.get_model().pass_structure_interner.get_structure(0).viewport.width,
              0.0f);
}

TEST_F(GLFrontendWindows, ADeviceContextOfNoKnownWindowHasNoSizeToTake)
{
    /* The context is made current to a device context that no GetDC of a window gave. */
    m_builder.create_window(WINDOW_ADDRESS,
                            POPUP_STYLE,
                            0, /* in_ex_style */
                            1280,
                            720);
    m_builder.make_current (DEVICE_CONTEXT_ADDRESS,
                            CONTEXT_ADDRESS);
    m_builder.clear        (COLOR_BUFFER_BIT);
    m_builder.swap_buffers (DEVICE_CONTEXT_ADDRESS);

    feed();

    EXPECT_EQ(m_harness.get_model().pass_structure_interner.get_structure(0).viewport.width,
              0.0f);
}

TEST_F(GLFrontendWindows, OnlyTheFirstMakeCurrentOfAContextSetsItsViewport)
{
    make_window_and_context(1280,
                            720);

    m_builder.viewport    (0,
                           0,
                           640,
                           360);
    m_builder.make_current(DEVICE_CONTEXT_ADDRESS,
                           0); /* unbind */
    m_builder.make_current(DEVICE_CONTEXT_ADDRESS,
                           CONTEXT_ADDRESS);
    m_builder.clear       (COLOR_BUFFER_BIT);
    m_builder.swap_buffers(DEVICE_CONTEXT_ADDRESS);

    feed();

    EXPECT_EQ(m_harness.get_model().pass_structure_interner.get_structure(0).viewport.width,
              640.0f);
}
