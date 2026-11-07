/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Checks of the OpenGL front end following the copies of the window to textures (render to a texture by a copy), and what depends on the size of
 * the window in pixels (the copies, the reads, the scissor boxes, the widths of lines and the sizes of points).
 */
#include <gtest/gtest.h>
#include "gl_frontend_fixture.h"

namespace
{
    class GlFrontendCopies : public Tests::GlFrontendFixture
    {
    protected:
        enum : uint32_t
        {
            GL_DEPTH_COMPONENT = 0x00001902,
            GL_RGBA            = 0x00001908,
            GL_TEXTURE_2D      = 0x00000DE1,
            GL_TRIANGLES       = 0x00000004
        };

        void add_triangle()
        {
            m_builder.begin    (GL_TRIANGLES);
            m_builder.vertex3fv({0.0f, 0.0f, -1.0f});
            m_builder.vertex3fv({1.0f, 0.0f, -1.0f});
            m_builder.vertex3fv({0.0f, 1.0f, -1.0f});
            m_builder.end      ();
        }

        /* The number of calls of the kind that depend on the size of the window. */
        uint64_t get_n_hazards(const Analysis::APIDumpAnalysis::ResolutionHazard& in_hazard)
        {
            const auto& hazard_map = m_harness.get_frontend().get_n_resolution_hazards();
            const auto  iterator   = hazard_map.find(in_hazard);

            return (iterator != hazard_map.end() ) ? iterator->second
                                                   : 0;
        }

        /* The accesses of the given role, in the order in which the model builder told them. */
        std::vector<Analysis::APIDumpAnalysis::AccessEvent> get_events(const Analysis::APIDumpAnalysis::AccessRole& in_role) const
        {
            std::vector<Analysis::APIDumpAnalysis::AccessEvent> result_vec;

            for (const auto& event : m_sink.event_vec)
            {
                if (event.role == in_role)
                {
                    result_vec.push_back(event);
                }
            }

            return result_vec;
        }

        /* The resource of the window that is the depth image (if in_is_depth) or the color one. */
        Analysis::APIDumpAnalysis::ResourceId get_window_resource(const bool& in_is_depth) const
        {
            Analysis::APIDumpAnalysis::ResourceId result = Analysis::APIDumpAnalysis::ResourceId::NONE;

            for (const auto& resource : m_harness.get_model().resource_vec)
            {
                if (resource.description.kind == Analysis::APIDumpAnalysis::ResourceKind::SWAP_CHAIN_IMAGE &&
                    in_is_depth               == false)
                {
                    result = resource.id;
                }
                else
                if (resource.description.is_depth_stencil &&
                    in_is_depth)
                {
                    result = resource.id;
                }
            }

            return result;
        }

        void SetUp() override
        {
            m_harness.get_builder().set_access_sink(&m_sink);
        }

        Tests::RecordingAccessSink m_sink;
    };
}

TEST_F(GlFrontendCopies, ACopyToATextureReadsTheWindowAndWritesTheTexture)
{
    start(640,
          480);

    m_builder.clear              (COLOR_BUFFER_BIT);
    m_builder.bind_texture       (GL_TEXTURE_2D,
                                  1);
    m_builder.tex_image_2d       (0,
                                  64,
                                  64,
                                  {});
    add_triangle                 ();
    m_builder.copy_tex_sub_image_2d(0,
                                    0,
                                    0,
                                    0,
                                    0,
                                    64,
                                    64);
    m_builder.swap_buffers       (DEVICE_CONTEXT_ADDRESS);

    feed();

    const auto sources      = get_events(Analysis::APIDumpAnalysis::AccessRole::TRANSFER_SOURCE);
    const auto destinations = get_events(Analysis::APIDumpAnalysis::AccessRole::TRANSFER_DESTINATION);

    ASSERT_EQ(sources.size(),
              1u);
    ASSERT_EQ(destinations.size(),
              1u);

    EXPECT_EQ(sources.at(0).resource_id,
              get_window_resource(false) );
    EXPECT_NE(destinations.at(0).resource_id,
              sources.at(0).resource_id);
    EXPECT_EQ(m_harness.get_frontend().get_n_unsupported_calls(),
              0u);
    EXPECT_EQ(m_harness.get_frontend().get_n_unfollowed_calls(),
              0u);
}

TEST_F(GlFrontendCopies, ACopyOfWhatWasDrawnEndsThePass)
{
    start(640,
          480);

    m_builder.clear                (COLOR_BUFFER_BIT);
    m_builder.bind_texture         (GL_TEXTURE_2D,
                                    1);
    m_builder.tex_image_2d         (0,
                                    64,
                                    64,
                                    {});
    add_triangle                   ();
    m_builder.copy_tex_sub_image_2d(0,
                                    0,
                                    0,
                                    0,
                                    0,
                                    64,
                                    64);
    add_triangle                   ();
    m_builder.swap_buffers         (DEVICE_CONTEXT_ADDRESS);

    feed();

    /* The clear and the first draw, then the draw after the copy. */
    EXPECT_EQ(m_harness.get_model().frame_record_vec.at(0).n_passes,
              2u);
}

TEST_F(GlFrontendCopies, ACopyIsAHazardOfTheSizeOfTheWindow)
{
    start(640,
          480);

    m_builder.bind_texture         (GL_TEXTURE_2D,
                                    1);
    m_builder.tex_image_2d         (0,
                                    64,
                                    64,
                                    {});
    m_builder.copy_tex_sub_image_2d(0,
                                    0,
                                    0,
                                    0,
                                    0,
                                    64,
                                    64);
    m_builder.copy_tex_image_2d    (0,
                                    GL_RGBA,
                                    0,
                                    0,
                                    32,
                                    32);

    feed();

    EXPECT_EQ(get_n_hazards(Analysis::APIDumpAnalysis::ResolutionHazard::COPIES_WITH_RECTANGLES),
              2u);
}

TEST_F(GlFrontendCopies, ACopyToAnImageGivesTheTextureItsSize)
{
    start(640,
          480);

    m_builder.bind_texture     (GL_TEXTURE_2D,
                                7);
    m_builder.copy_tex_image_2d(0,
                                GL_RGBA,
                                0,
                                0,
                                256,
                                128);

    feed();

    bool is_found = false;

    for (const auto& resource : m_harness.get_model().resource_vec)
    {
        if (resource.description.kind == Analysis::APIDumpAnalysis::ResourceKind::TEXTURE_2D &&
            resource.description.width == 256)
        {
            EXPECT_EQ(resource.description.height,
                      128u);

            is_found = true;
        }
    }

    EXPECT_TRUE(is_found);
    EXPECT_EQ(get_events(Analysis::APIDumpAnalysis::AccessRole::TRANSFER_DESTINATION).size(),
              1u);
}

TEST_F(GlFrontendCopies, ATextureOfDepthIsCopiedFromTheDepthOfTheWindow)
{
    start(640,
          480);

    m_builder.clear            (COLOR_BUFFER_BIT);
    m_builder.bind_texture     (GL_TEXTURE_2D,
                                1);
    m_builder.copy_tex_image_2d(0,
                                GL_DEPTH_COMPONENT,
                                0,
                                0,
                                64,
                                64);

    feed();

    const auto sources = get_events(Analysis::APIDumpAnalysis::AccessRole::TRANSFER_SOURCE);

    ASSERT_EQ(sources.size(),
              1u);

    EXPECT_EQ(sources.at(0).resource_id,
              get_window_resource(true) );
}

TEST_F(GlFrontendCopies, ACopyToATextureWithNoSizeIsNotFollowed)
{
    start(640,
          480);

    m_builder.bind_texture         (GL_TEXTURE_2D,
                                    1);
    m_builder.copy_tex_sub_image_2d(0,
                                    0,
                                    0,
                                    0,
                                    0,
                                    64,
                                    64);

    feed();

    EXPECT_EQ(m_harness.get_frontend().get_n_unfollowed_calls(),
              1u);
}

TEST_F(GlFrontendCopies, ATextureMadeByACopyIsSampledByTheDrawsAfterIt)
{
    start(640,
          480);

    m_builder.clear                (COLOR_BUFFER_BIT);
    m_builder.bind_texture         (GL_TEXTURE_2D,
                                    1);
    m_builder.copy_tex_image_2d    (0,
                                    GL_RGBA,
                                    0,
                                    0,
                                    64,
                                    64);
    m_builder.enable               (GL_TEXTURE_2D);
    add_triangle                   ();
    m_builder.swap_buffers         (DEVICE_CONTEXT_ADDRESS);

    feed();

    EXPECT_EQ(get_events(Analysis::APIDumpAnalysis::AccessRole::SAMPLED).size(),
              1u);
}

TEST_F(GlFrontendCopies, AReadOfPixelsIsAHazardAndIsNotFollowed)
{
    start(640,
          480);

    m_builder.read_pixels(15,
                          0,
                          1,
                          1);
    m_builder.read_pixels(30,
                          0,
                          1,
                          1);

    feed();

    EXPECT_EQ(get_n_hazards(Analysis::APIDumpAnalysis::ResolutionHazard::PIXEL_READBACKS),
              2u);
    EXPECT_EQ(m_harness.get_frontend().get_n_unsupported_calls(),
              2u);
}

TEST_F(GlFrontendCopies, ScissorBoxesLineWidthsAndPointSizesAreHazards)
{
    start(640,
          480);

    m_builder.scissor    (0,
                          50,
                          640,
                          380);
    m_builder.scissor    (0,
                          0,
                          640,
                          480);
    m_builder.line_width (2.0f);
    m_builder.point_size (3.0f);
    m_builder.point_size (4.0f);

    feed();

    EXPECT_EQ(get_n_hazards(Analysis::APIDumpAnalysis::ResolutionHazard::SCISSOR_BOXES),
              2u);
    EXPECT_EQ(get_n_hazards(Analysis::APIDumpAnalysis::ResolutionHazard::LINE_WIDTH_STATES),
              1u);
    EXPECT_EQ(get_n_hazards(Analysis::APIDumpAnalysis::ResolutionHazard::POINT_SIZE_STATES),
              2u);
}

TEST_F(GlFrontendCopies, OnlyAClearOfPartOfTheWindowIsAHazard)
{
    start(640,
          480);

    m_builder.clear  (COLOR_BUFFER_BIT);
    m_builder.scissor(0,
                      50,
                      640,
                      380);
    m_builder.clear  (COLOR_BUFFER_BIT); /* the test is off: all of the window */
    m_builder.enable (SCISSOR_TEST);
    m_builder.clear  (COLOR_BUFFER_BIT);

    feed();

    EXPECT_EQ(get_n_hazards(Analysis::APIDumpAnalysis::ResolutionHazard::CLEARS_WITH_RECTANGLES),
              1u);
}
