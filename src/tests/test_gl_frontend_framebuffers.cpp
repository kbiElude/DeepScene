/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * An application that makes its own framebuffer (a framebuffer object) is not followed: the front end has no handler for the calls that make, bind and fill one, so the
 * process ends naming the call, and nothing that the application renders to it is analysed or converted as if it went to the window. The converter relies on this: it
 * binds its own framebuffers for the eyes and never has to give the application's back.
 */
#include <gtest/gtest.h>
#include "common/macros.h"
#include "gl_frontend_fixture.h"
#include <stdlib.h>

namespace
{
    using namespace Tests;

    class GLFrontendFramebuffers : public GLFrontendFixture
    {
    protected:
        enum : uint32_t
        {
            GL_FRAMEBUFFER = 0x00008D40,
            GL_TRIANGLES   = 0x00000004
        };

        /* The application draws a block, so that the call that is not followed comes from a program that was being followed. */
        void add_a_draw_and_the_call_of(const APIInterceptor::APIFunction& in_function)
        {
            start(640,
                  480);

            m_builder.begin    (GL_TRIANGLES);
            m_builder.vertex3fv({0.0f, 0.0f, -1.0f});
            m_builder.vertex3fv({1.0f, 0.0f, -1.0f});
            m_builder.vertex3fv({0.0f, 1.0f, -1.0f});
            m_builder.end      ();

            m_builder.add_call(in_function,
                               {APIInterceptor::APIFunctionArgument::create_u32_glenum(GL_FRAMEBUFFER),
                                APIInterceptor::APIFunctionArgument::create_i32       (1)});
        }

        /* The calls that were added end the process, and say which call is not followed. */
        void expect_refused(const char* in_pattern_ptr)
        {
            /* The process has no console when the tests run from a tool, and would wait for somebody to close the box. */
            ::_putenv_s("APIINTERCEPTOR_NO_ERROR_DIALOG",
                        "1");

            EXPECT_EXIT(feed(),
                        ::testing::ExitedWithCode(APIInterceptor::FatalError::get_exit_code() ),
                        in_pattern_ptr);
        }
    };
}

TEST_F(GLFrontendFramebuffers, BindingAFramebufferObjectIsRefused)
{
    add_a_draw_and_the_call_of(APIInterceptor::APIFUNCTION_GL_GLBINDFRAMEBUFFER);

    expect_refused("glBindFramebuffer");
}

TEST_F(GLFrontendFramebuffers, MakingAFramebufferObjectIsRefused)
{
    add_a_draw_and_the_call_of(APIInterceptor::APIFUNCTION_GL_GLGENFRAMEBUFFERS);

    expect_refused("glGenFramebuffers");
}

TEST_F(GLFrontendFramebuffers, DeletingAFramebufferObjectIsRefused)
{
    add_a_draw_and_the_call_of(APIInterceptor::APIFUNCTION_GL_GLDELETEFRAMEBUFFERS);

    expect_refused("glDeleteFramebuffers");
}

TEST_F(GLFrontendFramebuffers, AttachingARenderbufferIsRefused)
{
    add_a_draw_and_the_call_of(APIInterceptor::APIFUNCTION_GL_GLFRAMEBUFFERRENDERBUFFER);

    expect_refused("glFramebufferRenderbuffer");
}

TEST_F(GLFrontendFramebuffers, AttachingATextureIsRefused)
{
    add_a_draw_and_the_call_of(APIInterceptor::APIFUNCTION_GL_GLFRAMEBUFFERTEXTURE2D);

    expect_refused("glFramebufferTexture2D");
}

TEST_F(GLFrontendFramebuffers, MakingARenderbufferIsRefused)
{
    add_a_draw_and_the_call_of(APIInterceptor::APIFUNCTION_GL_GLGENRENDERBUFFERS);

    expect_refused("glGenRenderbuffers");
}

TEST_F(GLFrontendFramebuffers, GivingARenderbufferItsStorageIsRefused)
{
    add_a_draw_and_the_call_of(APIInterceptor::APIFUNCTION_GL_GLRENDERBUFFERSTORAGE);

    expect_refused("glRenderbufferStorage");
}

TEST_F(GLFrontendFramebuffers, CopyingBetweenFramebuffersIsRefused)
{
    add_a_draw_and_the_call_of(APIInterceptor::APIFUNCTION_GL_GLBLITFRAMEBUFFER);

    expect_refused("glBlitFramebuffer");
}
