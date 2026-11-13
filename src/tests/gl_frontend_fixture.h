/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * What the tests of the OpenGL front end have in common: a call builder, a front end with a model builder to run its calls through, and
 * the values that the calls use.
 */
#ifndef TESTS_GL_FRONTEND_FIXTURE_H
#define TESTS_GL_FRONTEND_FIXTURE_H

#include <gtest/gtest.h>
#include "analysis/api_dump_analysis/frontends/OpenGL/opengl_frontend.h"
#include "analysis/api_dump_analysis/i_access_sink.h"
#include "frontend_harness.h"
#include "gl_call_builder.h"
#include <stdint.h>
#include <vector>

namespace Tests
{
    /* Keeps the accesses that the model builder tells of. */
    class RecordingAccessSink : public Analysis::APIDumpAnalysis::IAccessSink
    {
    public:
        void on_access(const Analysis::APIDumpAnalysis::AccessEvent& in_event) override
        {
            event_vec.push_back(in_event);
        }

        void on_frame_ended(const uint64_t& in_frame_index,
                            const uint64_t& in_first_call_index,
                            const uint64_t& in_last_call_index) override
        {
            (void) in_frame_index;
            (void) in_first_call_index;
            (void) in_last_call_index;
        }

        std::vector<Analysis::APIDumpAnalysis::AccessEvent> event_vec;
    };

    class GLFrontendFixture : public ::testing::Test
    {
    protected:
        typedef FrontendHarness<Analysis::APIDumpAnalysis::Frontends::OpenGLFrontend,
                                GLCallBuilder>                                       Harness;

        /* Addresses of the objects of a program. */
        enum : uint64_t
        {
            CONTEXT_ADDRESS        = 0x10000,
            DEVICE_CONTEXT_ADDRESS = 0x63012365,
            OTHER_CONTEXT_ADDRESS  = 0x10001
        };

        /* The values of OpenGL that the calls of the tests use. */
        enum : uint32_t
        {
            ALL_ATTRIB_BITS    = 0x000FFFFF,
            COLOR_BUFFER_BIT   = 0x00004000,
            DEPTH_BUFFER_BIT   = 0x00000100,
            MODELVIEW          = 0x00001700,
            PROJECTION         = 0x00001701,
            QUADS              = 0x00000007,
            SCISSOR_TEST       = 0x00000C11,
            STENCIL_BUFFER_BIT = 0x00000400,
            TEXTURE            = 0x00001702
        };

        void feed()
        {
            m_harness.feed(m_builder);
        }

        /* A context which is current, and the viewport that an application sets first. */
        void start(const int32_t& in_width,
                   const int32_t& in_height)
        {
            m_builder.make_current(DEVICE_CONTEXT_ADDRESS,
                                   CONTEXT_ADDRESS);
            m_builder.viewport    (0,
                                   0,
                                   in_width,
                                   in_height);
        }

        /* The matrices of the context which the thread has current (the first thread, which is the one of the builder at first). */
        void read_matrices()
        {
            ASSERT_TRUE(m_harness.get_frontend().get_current_matrices(1,
                                                                     &m_modelview_f32mat,
                                                                     &m_projection_f32mat) );
        }

        GLCallBuilder                                  m_builder;
        Harness                                        m_harness;
        Analysis::APIDumpAnalysis::Frontends::GLMatrix m_modelview_f32mat;
        Analysis::APIDumpAnalysis::Frontends::GLMatrix m_projection_f32mat;
    };
}

#endif /* TESTS_GL_FRONTEND_FIXTURE_H */
