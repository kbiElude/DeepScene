/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Checks of the OpenGL front end following textures: a texture of the program is a resource once the first level gives it a size, a draw samples
 * the textures of the units that have the target enabled, and what a unit has bound is a part of the attributes that glPushAttrib saves.
 */
#include <gtest/gtest.h>
#include "common/macros.h"
#include "gl_frontend_fixture.h"
#include <stdlib.h>

namespace
{
    class GlFrontendTextures : public Tests::GlFrontendFixture
    {
    protected:
        enum : uint32_t
        {
            GL_TEXTURE_1D = 0x00000DE0,
            GL_TEXTURE_2D = 0x00000DE1,
            GL_TRIANGLES  = 0x00000004
        };

        /* A triangle, as the program draws it. */
        void add_triangle()
        {
            m_builder.begin    (GL_TRIANGLES);
            m_builder.vertex3fv({0.0f, 0.0f, -1.0f});
            m_builder.vertex3fv({1.0f, 0.0f, -1.0f});
            m_builder.vertex3fv({0.0f, 1.0f, -1.0f});
            m_builder.end      ();
        }

        /* A texture of the given size, bound to the active unit. */
        void make_texture(const uint32_t& in_name,
                          const int32_t&  in_size)
        {
            m_builder.bind_texture(GL_TEXTURE_2D,
                                   in_name);
            m_builder.tex_image_2d(0,
                                   in_size,
                                   in_size,
                                   {});
        }

        /* The number of draws, in the model, that sampled a texture. */
        size_t count_sampled_draws() const
        {
            size_t result = 0;

            for (const auto& event : m_sink.event_vec)
            {
                if (event.role == Analysis::APIDumpAnalysis::AccessRole::SAMPLED)
                {
                    ++result;
                }
            }

            return result;
        }

        size_t count_textures() const
        {
            size_t result = 0;

            for (const auto& resource : m_harness.get_model().resource_vec)
            {
                if (resource.description.kind == Analysis::APIDumpAnalysis::ResourceKind::TEXTURE_2D)
                {
                    ++result;
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

TEST_F(GlFrontendTextures, TheFirstLevelOfATextureMakesItAResourceOfItsSize)
{
    start(640,
          480);

    make_texture(1,
                 64);

    feed();

    ASSERT_EQ(count_textures(),
              1u);

    for (const auto& resource : m_harness.get_model().resource_vec)
    {
        if (resource.description.kind == Analysis::APIDumpAnalysis::ResourceKind::TEXTURE_2D)
        {
            EXPECT_EQ(resource.description.width,
                      64u);
            EXPECT_EQ(resource.description.height,
                      64u);
        }
    }
}

TEST_F(GlFrontendTextures, ATextureIsSampledByADrawWhenItsUnitHasTheTargetEnabled)
{
    start(640,
          480);

    m_builder.clear       (COLOR_BUFFER_BIT);
    make_texture          (1,
                           64);
    add_triangle          (); /* the target is not enabled */
    m_builder.enable      (GL_TEXTURE_2D);
    add_triangle          ();
    m_builder.swap_buffers(DEVICE_CONTEXT_ADDRESS);

    feed();

    EXPECT_EQ(count_sampled_draws(),
              1u);
    EXPECT_EQ(m_harness.get_model().frame_record_vec.at(0).n_draws,
              2u);
}

TEST_F(GlFrontendTextures, ADrawWithNoTextureBoundSamplesNone)
{
    start(640,
          480);

    m_builder.clear       (COLOR_BUFFER_BIT);
    m_builder.enable      (GL_TEXTURE_2D);
    add_triangle          ();
    m_builder.swap_buffers(DEVICE_CONTEXT_ADDRESS);

    feed();

    EXPECT_EQ(count_sampled_draws(),
              0u);
}

TEST_F(GlFrontendTextures, TheTargetIsEnabledForTheActiveUnitOnly)
{
    start(640,
          480);

    m_builder.clear         (COLOR_BUFFER_BIT);
    make_texture            (1,
                             64);
    m_builder.active_texture(1);
    m_builder.enable        (GL_TEXTURE_2D); /* the second unit has it enabled, and no texture */
    m_builder.active_texture(0);
    add_triangle            ();
    m_builder.swap_buffers  (DEVICE_CONTEXT_ADDRESS);

    feed();

    EXPECT_EQ(count_sampled_draws(),
              0u);
}

TEST_F(GlFrontendTextures, EachUnitSamplesItsOwnTexture)
{
    start(640,
          480);

    m_builder.clear         (COLOR_BUFFER_BIT);
    make_texture            (1,
                             64);
    m_builder.enable        (GL_TEXTURE_2D);
    m_builder.active_texture(1);
    make_texture            (2,
                             32);
    m_builder.enable        (GL_TEXTURE_2D);
    add_triangle            ();
    m_builder.swap_buffers  (DEVICE_CONTEXT_ADDRESS);

    feed();

    EXPECT_EQ(count_sampled_draws(),
              2u);
}

TEST_F(GlFrontendTextures, DrawsWithTexturesOfAnotherSizeAreOfAnotherStructure)
{
    start(640,
          480);

    m_builder.clear       (COLOR_BUFFER_BIT);
    m_builder.enable      (GL_TEXTURE_2D);
    make_texture          (1,
                           64);
    add_triangle          ();
    make_texture          (2,
                           64);
    add_triangle          (); /* another texture, the same kind */
    make_texture          (3,
                           32);
    add_triangle          ();
    m_builder.swap_buffers(DEVICE_CONTEXT_ADDRESS);

    feed();

    EXPECT_EQ(m_harness.get_model().draw_structure_map.size(),
              2u);
}

TEST_F(GlFrontendTextures, PixelsInACallAreAnUpload)
{
    start(640,
          480);

    m_builder.bind_texture(GL_TEXTURE_2D,
                           1);
    m_builder.tex_image_2d(0,
                           2,
                           2,
                           std::vector<uint8_t>(2 * 2 * 4,
                                                0x7F) );

    feed();

    size_t n_uploads = 0;

    for (const auto& event : m_sink.event_vec)
    {
        if (event.role == Analysis::APIDumpAnalysis::AccessRole::UPLOAD_DESTINATION)
        {
            ++n_uploads;
        }
    }

    EXPECT_EQ(n_uploads,
              1u);
}

TEST_F(GlFrontendTextures, ALevelWithNoPixelsIsNotAnUpload)
{
    start(640,
          480);

    make_texture(1,
                 64);

    feed();

    for (const auto& event : m_sink.event_vec)
    {
        EXPECT_NE(event.role,
                  Analysis::APIDumpAnalysis::AccessRole::UPLOAD_DESTINATION);
    }
}

TEST_F(GlFrontendTextures, ATextureGivenAnotherSizeIsAnotherResource)
{
    start(640,
          480);

    make_texture(1,
                 64);
    make_texture(1,
                 64); /* the same size: the same resource */
    make_texture(1,
                 32);

    feed();

    const auto& resource_vec = m_harness.get_model().resource_vec;

    EXPECT_EQ(count_textures(),
              2u);

    size_t n_destroyed = 0;

    for (const auto& resource : resource_vec)
    {
        if (resource.description.kind == Analysis::APIDumpAnalysis::ResourceKind::TEXTURE_2D &&
            resource.is_destroyed)
        {
            ++n_destroyed;
        }
    }

    EXPECT_EQ(n_destroyed,
              1u);
}

TEST_F(GlFrontendTextures, ADeletedTextureIsGoneAndNotSampled)
{
    start(640,
          480);

    m_builder.clear        (COLOR_BUFFER_BIT);
    m_builder.enable       (GL_TEXTURE_2D);
    make_texture           (1,
                            64);
    m_builder.delete_textures({1});
    add_triangle           ();
    m_builder.swap_buffers (DEVICE_CONTEXT_ADDRESS);

    feed();

    EXPECT_EQ(count_sampled_draws(),
              0u);
    EXPECT_EQ(m_harness.get_frontend().get_n_unfollowed_calls(),
              0u);

    for (const auto& resource : m_harness.get_model().resource_vec)
    {
        /* The depth image of the window is a texture too, and is not the program's. */
        if (resource.description.kind             == Analysis::APIDumpAnalysis::ResourceKind::TEXTURE_2D &&
            resource.description.is_depth_stencil == false)
        {
            EXPECT_TRUE(resource.is_destroyed);
        }
    }
}

TEST_F(GlFrontendTextures, APopPutsBackTheTextureThatWasBound)
{
    start(640,
          480);

    m_builder.clear       (COLOR_BUFFER_BIT);
    m_builder.enable      (GL_TEXTURE_2D);
    make_texture          (1,
                           64);
    make_texture          (2,
                           32);
    m_builder.bind_texture(GL_TEXTURE_2D,
                           1);
    add_triangle          ();
    m_builder.push_attrib (ALL_ATTRIB_BITS);
    m_builder.bind_texture(GL_TEXTURE_2D,
                           2);
    m_builder.pop_attrib  ();
    add_triangle          (); /* the first texture again */
    m_builder.swap_buffers(DEVICE_CONTEXT_ADDRESS);

    feed();

    EXPECT_EQ(m_harness.get_model().draw_structure_map.size(),
              1u);
}

TEST_F(GlFrontendTextures, TheDefaultTextureOfATargetThatIsNotFollowedIsNotCounted)
{
    start(640,
          480);

    m_builder.bind_texture(GL_TEXTURE_1D,
                           0);

    feed();

    EXPECT_EQ(m_harness.get_frontend().get_n_unsupported_calls(),
              0u);
}

TEST_F(GlFrontendTextures, ATargetThatIsNotFollowedEndsTheProcess)
{
    start(640,
          480);

    m_builder.bind_texture(GL_TEXTURE_1D,
                           1);

    /* The process has no console when the tests run from a tool, and would wait for somebody to close the box. */
    ::_putenv_s("APIINTERCEPTOR_NO_ERROR_DIALOG",
                "1");

    EXPECT_EXIT(feed(),
                ::testing::ExitedWithCode(APIInterceptor::FatalError::get_exit_code() ),
                "glBindTexture.*GL_TEXTURE_2D");
}

TEST_F(GlFrontendTextures, ATextureIsFoundByItsNameOnceItHasASize)
{
    start(640,
          480);

    /* A name that the program has made and not given a size has no resource yet. */
    m_builder.bind_texture(GL_TEXTURE_2D,
                           7);

    feed();

    Analysis::APIDumpAnalysis::ResourceView view;

    EXPECT_FALSE(m_harness.get_frontend().find_resource_view(7,
                                                            &view) );

    make_texture(7,
                 64);

    feed();

    ASSERT_TRUE(m_harness.get_frontend().find_resource_view(7,
                                                           &view) );
    EXPECT_NE   (view.resource_id,
                 Analysis::APIDumpAnalysis::ResourceId::NONE);
    EXPECT_EQ   (view.mip_level,
                 0u);
    EXPECT_FALSE(m_harness.get_frontend().find_resource_view(8,
                                                            &view) );
}
