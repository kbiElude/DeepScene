/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Checks of what the OpenGL front end tells of the textures (see ITextureSink): an upload with the bytes it carries and how they are laid out, a part of a level,
 * the levels a texture is given, what is copied from the window, and a texture that is made again.
 */
#include <gtest/gtest.h>
#include "analysis/api_dump_textures/i_texture_sink.h"
#include "analysis/api_dump_textures/texture_blob_codec.h"
#include "analysis/api_dump_textures/texture_blob_writer.h"
#include "analysis/api_dump_textures/texture_collector.h"
#include "gl_frontend_fixture.h"
#include "temp_directory.h"

namespace
{
    typedef Analysis::APIDumpTextures::TextureEvent     TextureEvent;
    typedef Analysis::APIDumpTextures::TextureEventKind TextureEventKind;

    /* Keeps what the model builder tells of the textures, with a copy of the bytes. */
    class RecordingTextureSink : public Analysis::APIDumpTextures::ITextureSink
    {
    public:
        void on_texture_event(const TextureEvent& in_event,
                              const uint8_t*      in_data_ptr,
                              const size_t&       in_n_data_bytes,
                              const uint32_t&     in_data_chunk_id) override
        {
            event_vec.push_back(in_event);

            data_chunk_id_vec.push_back(in_data_chunk_id);
            has_data_vec.push_back    (in_data_ptr != nullptr);

            data_vec.push_back(std::vector<uint8_t>() );

            if (in_data_ptr != nullptr)
            {
                data_vec.back().assign(in_data_ptr,
                                       in_data_ptr + in_n_data_bytes);
            }
        }

        std::vector<uint32_t>             data_chunk_id_vec;
        std::vector<std::vector<uint8_t> > data_vec;
        std::vector<TextureEvent>         event_vec;
        std::vector<bool>                 has_data_vec;
    };

    class GlFrontendTextureEvents : public Tests::GlFrontendFixture
    {
    protected:
        enum : uint32_t
        {
            GL_RGBA                  = 0x00001908,
            GL_TEXTURE_2D            = 0x00000DE1,
            GL_UNPACK_ALIGNMENT      = 0x00000CF5,
            GL_UNPACK_ROW_LENGTH     = 0x00000CF2,
            GL_UNSIGNED_BYTE         = 0x00001401
        };

        void SetUp() override
        {
            m_harness.get_builder().set_texture_sink(&m_sink);

            start(64,
                  64);
        }

        /* Bytes that are told apart: the first is the number given. */
        static std::vector<uint8_t> make_pixels(const size_t&  in_n_bytes,
                                                const uint8_t& in_first)
        {
            std::vector<uint8_t> result_vec(in_n_bytes);

            for (size_t n_byte = 0; n_byte < in_n_bytes; ++n_byte)
            {
                result_vec.at(n_byte) = static_cast<uint8_t>(in_first + n_byte);
            }

            return result_vec;
        }

        RecordingTextureSink m_sink;
    };
}

TEST_F(GlFrontendTextureEvents, ALevelThatIsGivenPixelsIsAnUploadWithTheBytesAndHowTheyAreLaidOut)
{
    const auto pixels = make_pixels(4 * 4 * 4,
                                    10);

    m_builder.bind_texture(GL_TEXTURE_2D,
                           5);
    m_builder.tex_image_2d(0,
                           4,
                           4,
                           pixels);

    feed();

    ASSERT_EQ(m_sink.event_vec.size(),
              1u);

    const TextureEvent& event = m_sink.event_vec.at(0);

    EXPECT_EQ(event.kind,
              TextureEventKind::UPLOAD);
    EXPECT_NE(event.destination.resource_id,
              Analysis::APIDumpAnalysis::ResourceId::NONE);
    EXPECT_EQ(event.destination.mip_level,
              0u);
    EXPECT_EQ(event.layer_count,
              1u);

    /* The region is the whole level, in texels, and the level is as the call made it, in the words of the project. */
    EXPECT_EQ(event.x,
              0u);
    EXPECT_EQ(event.y,
              0u);
    EXPECT_EQ(event.width,
              4u);
    EXPECT_EQ(event.height,
              4u);
    EXPECT_EQ(event.level_width,
              4u);
    EXPECT_EQ(event.level_height,
              4u);
    EXPECT_EQ(event.level_format,
              Utils::Graphics::TextureFormat::R8G8B8A8_UNORM);

    /* How the bytes are laid out: the format and the type of the call, and rows of four texels of four bytes. */
    EXPECT_EQ(event.layout.api_format,
              static_cast<uint32_t>(GL_RGBA) );
    EXPECT_EQ(event.layout.api_type,
              static_cast<uint32_t>(GL_UNSIGNED_BYTE) );
    EXPECT_EQ(event.layout.row_pitch_n_bytes,
              16u);

    ASSERT_TRUE(m_sink.has_data_vec.at(0) );

    EXPECT_EQ(m_sink.data_vec.at(0),
              pixels);
}

TEST_F(GlFrontendTextureEvents, ALevelThatIsGivenNoPixelsIsAnUploadWithNoData)
{
    m_builder.bind_texture(GL_TEXTURE_2D,
                           5);
    m_builder.tex_image_2d(0,
                           8,
                           8,
                           {});

    feed();

    ASSERT_EQ(m_sink.event_vec.size(),
              1u);

    EXPECT_EQ(m_sink.event_vec.at(0).kind,
              TextureEventKind::UPLOAD);
    EXPECT_FALSE(m_sink.has_data_vec.at(0) );
}

TEST_F(GlFrontendTextureEvents, TheRowsOfAnUploadAreAsLongAsThePixelStoreSays)
{
    m_builder.bind_texture(GL_TEXTURE_2D,
                           5);

    /* Rows of three texels of four bytes, each starting at a multiple of eight. */
    m_builder.pixel_store(GL_UNPACK_ALIGNMENT,
                          8);
    m_builder.tex_image_2d(0,
                           3,
                           3,
                           {});

    /* And with a length of the rows of ten texels. */
    m_builder.pixel_store(GL_UNPACK_ROW_LENGTH,
                          10);
    m_builder.tex_image_2d(0,
                           3,
                           3,
                           {});

    feed();

    ASSERT_EQ(m_sink.event_vec.size(),
              2u);

    EXPECT_EQ(m_sink.event_vec.at(0).layout.row_pitch_n_bytes,
              16u);
    EXPECT_EQ(m_sink.event_vec.at(1).layout.row_pitch_n_bytes,
              40u);
}

TEST_F(GlFrontendTextureEvents, APartOfALevelIsAnUploadOfTheRegionAndTheLevelIsAsItWas)
{
    const auto pixels = make_pixels(2 * 3 * 4,
                                    50);

    m_builder.bind_texture(GL_TEXTURE_2D,
                           5);
    m_builder.tex_image_2d(0,
                           8,
                           8,
                           {});
    m_builder.tex_sub_image_2d(0,
                               2,
                               4,
                               2,
                               3,
                               pixels);

    feed();

    ASSERT_EQ(m_sink.event_vec.size(),
              2u);

    const TextureEvent& event = m_sink.event_vec.at(1);

    EXPECT_EQ(event.kind,
              TextureEventKind::UPLOAD);
    EXPECT_EQ(event.x,
              2u);
    EXPECT_EQ(event.y,
              4u);
    EXPECT_EQ(event.width,
              2u);
    EXPECT_EQ(event.height,
              3u);
    EXPECT_EQ(event.level_width,
              8u);
    EXPECT_EQ(event.level_height,
              8u);
    EXPECT_EQ(event.layout.row_pitch_n_bytes,
              8u);
    EXPECT_EQ(m_sink.data_vec.at(1),
              pixels);

    /* The same resource as the level that it is a part of. */
    EXPECT_EQ(event.destination.resource_id,
              m_sink.event_vec.at(0).destination.resource_id);
}

TEST_F(GlFrontendTextureEvents, APartOfALevelThatTheTextureHasNotBeenGivenIsNotTold)
{
    m_builder.bind_texture(GL_TEXTURE_2D,
                           5);
    m_builder.tex_image_2d(0,
                           8,
                           8,
                           {});
    m_builder.tex_sub_image_2d(3,
                               0,
                               0,
                               2,
                               2,
                               make_pixels(16,
                                           1) );

    feed();

    EXPECT_EQ(m_sink.event_vec.size(),
              1u);
}

TEST_F(GlFrontendTextureEvents, EachLevelHasTheSizeThatItWasGiven)
{
    m_builder.bind_texture(GL_TEXTURE_2D,
                           5);
    m_builder.tex_image_2d(0,
                           8,
                           8,
                           {});
    m_builder.tex_image_2d(1,
                           4,
                           4,
                           {});
    m_builder.tex_sub_image_2d(1,
                               0,
                               0,
                               2,
                               2,
                               make_pixels(16,
                                           1) );

    feed();

    ASSERT_EQ(m_sink.event_vec.size(),
              3u);

    EXPECT_EQ(m_sink.event_vec.at(1).destination.mip_level,
              1u);
    EXPECT_EQ(m_sink.event_vec.at(1).level_width,
              4u);

    /* A part of the level that is of the size of the level and not of the first. */
    EXPECT_EQ(m_sink.event_vec.at(2).destination.mip_level,
              1u);
    EXPECT_EQ(m_sink.event_vec.at(2).level_width,
              4u);
    EXPECT_EQ(m_sink.event_vec.at(2).level_height,
              4u);
}

TEST_F(GlFrontendTextureEvents, WhatIsCopiedFromTheWindowIsContentThatRenderingMadeAndHasNoSource)
{
    m_builder.bind_texture(GL_TEXTURE_2D,
                           5);
    m_builder.copy_tex_image_2d(0,
                                GL_RGBA,
                                0,
                                0,
                                16,
                                16);
    m_builder.copy_tex_sub_image_2d(0,
                                    4,
                                    8,
                                    0,
                                    0,
                                    6,
                                    2);

    feed();

    ASSERT_EQ(m_sink.event_vec.size(),
              2u);

    const TextureEvent& whole  = m_sink.event_vec.at(0);
    const TextureEvent& region = m_sink.event_vec.at(1);

    EXPECT_EQ(whole.kind,
              TextureEventKind::RENDER_WRITE);
    EXPECT_EQ(whole.width,
              16u);
    EXPECT_EQ(whole.height,
              16u);
    EXPECT_EQ(whole.level_format,
              Utils::Graphics::TextureFormat::R8G8B8A8_UNORM);
    EXPECT_EQ(whole.source.resource_id,
              Analysis::APIDumpAnalysis::ResourceId::NONE);
    EXPECT_FALSE(m_sink.has_data_vec.at(0) );

    EXPECT_EQ(region.kind,
              TextureEventKind::RENDER_WRITE);
    EXPECT_EQ(region.x,
              4u);
    EXPECT_EQ(region.y,
              8u);
    EXPECT_EQ(region.width,
              6u);
    EXPECT_EQ(region.height,
              2u);
    EXPECT_EQ(region.level_width,
              16u);
}

TEST_F(GlFrontendTextureEvents, ATextureThatIsGivenAnotherSizeIsAnotherResourceAndTheLevelIsAsTheLastCallMadeIt)
{
    m_builder.bind_texture(GL_TEXTURE_2D,
                           5);
    m_builder.tex_image_2d(0,
                           8,
                           8,
                           {});
    m_builder.tex_image_2d(0,
                           16,
                           4,
                           {});

    feed();

    ASSERT_EQ(m_sink.event_vec.size(),
              2u);

    EXPECT_NE(m_sink.event_vec.at(0).destination.resource_id,
              m_sink.event_vec.at(1).destination.resource_id);
    EXPECT_EQ(m_sink.event_vec.at(0).level_width,
              8u);
    EXPECT_EQ(m_sink.event_vec.at(1).level_width,
              16u);
    EXPECT_EQ(m_sink.event_vec.at(1).level_height,
              4u);
}

TEST_F(GlFrontendTextureEvents, EachEventHasTheCallAndTheFrameItIsIn)
{
    m_builder.bind_texture(GL_TEXTURE_2D,
                           5);
    m_builder.tex_image_2d(0,
                           4,
                           4,
                           {});
    m_builder.swap_buffers(DEVICE_CONTEXT_ADDRESS);
    m_builder.tex_sub_image_2d(0,
                               0,
                               0,
                               2,
                               2,
                               make_pixels(16,
                                           1) );

    feed();

    ASSERT_EQ(m_sink.event_vec.size(),
              2u);

    EXPECT_EQ(m_sink.event_vec.at(0).frame_index,
              0u);
    EXPECT_EQ(m_sink.event_vec.at(1).frame_index,
              1u);
    EXPECT_LT(m_sink.event_vec.at(0).call_index,
              m_sink.event_vec.at(1).call_index);
}

TEST_F(GlFrontendTextureEvents, TheCollectorKeepsTheEventsAndLeavesTheBytesInTheWorkload)
{
    Tests::TempDirectory                              directory;
    Analysis::APIDumpTextures::TextureBlobWriter      writer;
    Analysis::APIDumpTextures::TextureCollector       collector(&writer);
    std::string                                       error;
    const auto                                        pixels = make_pixels(16,
                                                                           3);

    ASSERT_TRUE(writer.open(directory.get_path() + "\\blobs.data",
                           &error) ) << error;

    m_harness.get_builder().set_texture_sink(&collector);

    m_builder.bind_texture(GL_TEXTURE_2D,
                           5);
    m_builder.tex_image_2d(0,
                           2,
                           2,
                           pixels);
    m_builder.tex_image_2d(1,
                           1,
                           1,
                           {});

    feed();

    const auto& event_vec = collector.get_event_vec();

    ASSERT_EQ(event_vec.size(),
              2u);

    /* The upload that has bytes has a place in the table, and the one that has none has not. */
    ASSERT_NE(event_vec.at(0).blob_index,
              Analysis::APIDumpTextures::NO_BLOB);
    EXPECT_EQ(event_vec.at(1).blob_index,
              Analysis::APIDumpTextures::NO_BLOB);

    const auto& entry = writer.get_blob_vec().at(event_vec.at(0).blob_index);

    uint64_t content_hash = 0;
    uint64_t length_hash  = 0;

    Analysis::APIDumpTextures::TextureBlobCodec::compute_hashes(pixels.data(),
                                                                pixels.size(),
                                                               &content_hash,
                                                               &length_hash);

    EXPECT_EQ(entry.location,
              Analysis::APIDumpTextures::BlobLocation::WORKLOAD_CHUNK);
    EXPECT_EQ(entry.content_hash,
              content_hash);
    EXPECT_EQ(entry.length_hash,
              length_hash);
    EXPECT_EQ(entry.n_raw_bytes,
              pixels.size() );
    EXPECT_TRUE(collector.get_error().empty() );

    /* Nothing of the bytes is in the file of blobs. */
    EXPECT_EQ(writer.get_n_bytes(),
              16u);
}

namespace
{
    class GlFrontendTextureSnapshots : public GlFrontendTextureEvents
    {
    protected:
        /* A triangle, drawn with whatever the units have. */
        void add_draw()
        {
            m_builder.begin    (GL_TRIANGLES);
            m_builder.vertex3fv({0.0f, 0.0f, -1.0f});
            m_builder.vertex3fv({1.0f, 0.0f, -1.0f});
            m_builder.vertex3fv({0.0f, 1.0f, -1.0f});
            m_builder.end      ();
        }

        /* The events of a kind. */
        size_t count_events(const TextureEventKind& in_kind) const
        {
            size_t result = 0;

            for (const auto& event : m_sink.event_vec)
            {
                result += (event.kind == in_kind) ? 1 : 0;
            }

            return result;
        }

        enum : uint32_t
        {
            GL_TRIANGLES = 0x00000004
        };
    };
}

TEST_F(GlFrontendTextureSnapshots, ALevelThatRenderingMadeIsSnapshotWhenADrawFirstReadsItAndNotAgain)
{
    m_builder.bind_texture(GL_TEXTURE_2D,
                           5);
    m_builder.enable      (GL_TEXTURE_2D);
    m_builder.copy_tex_image_2d(0,
                                GL_RGBA,
                                0,
                                0,
                                16,
                                8);

    add_draw();
    add_draw();

    feed();

    /* The copy, then the one snapshot that the first draw makes: the second finds nothing changed. */
    ASSERT_EQ(m_sink.event_vec.size(),
              2u);

    const TextureEvent& snapshot = m_sink.event_vec.at(1);

    EXPECT_EQ(snapshot.kind,
              TextureEventKind::SNAPSHOT);
    EXPECT_EQ(snapshot.destination.resource_id,
              m_sink.event_vec.at(0).destination.resource_id);
    EXPECT_EQ(snapshot.x,
              0u);
    EXPECT_EQ(snapshot.width,
              16u);
    EXPECT_EQ(snapshot.height,
              8u);
    EXPECT_EQ(snapshot.level_format,
              Utils::Graphics::TextureFormat::R8G8B8A8_UNORM);
    EXPECT_FALSE(m_sink.has_data_vec.at(1) );
    EXPECT_GT(snapshot.call_index,
              m_sink.event_vec.at(0).call_index);
}

TEST_F(GlFrontendTextureSnapshots, ALevelThatIsChangedAgainIsSnapshotAgainWhenItIsRead)
{
    m_builder.bind_texture(GL_TEXTURE_2D,
                           5);
    m_builder.enable      (GL_TEXTURE_2D);
    m_builder.copy_tex_image_2d(0,
                                GL_RGBA,
                                0,
                                0,
                                8,
                                8);

    add_draw();

    m_builder.copy_tex_sub_image_2d(0,
                                    0,
                                    0,
                                    0,
                                    0,
                                    4,
                                    4);

    add_draw();

    feed();

    EXPECT_EQ(count_events(TextureEventKind::RENDER_WRITE),
              2u);
    EXPECT_EQ(count_events(TextureEventKind::SNAPSHOT),
              2u);
}

TEST_F(GlFrontendTextureSnapshots, ATextureThatRenderingDidNotChangeIsNotSnapshot)
{
    m_builder.bind_texture(GL_TEXTURE_2D,
                           5);
    m_builder.enable      (GL_TEXTURE_2D);
    m_builder.tex_image_2d(0,
                           4,
                           4,
                           make_pixels(64,
                                       1) );

    add_draw();

    feed();

    EXPECT_EQ(count_events(TextureEventKind::SNAPSHOT),
              0u);
}

TEST_F(GlFrontendTextureSnapshots, ATextureThatNothingSamplesIsNotRead)
{
    m_builder.bind_texture(GL_TEXTURE_2D,
                           5);
    m_builder.copy_tex_image_2d(0,
                                GL_RGBA,
                                0,
                                0,
                                8,
                                8);

    /* The target is not enabled on the unit: the draw does not read it. */
    add_draw();

    feed();

    EXPECT_EQ(count_events(TextureEventKind::SNAPSHOT),
              0u);

    /* Enabled, it is read, and that is when it is snapshot. */
    m_builder.enable(GL_TEXTURE_2D);

    add_draw();

    feed();

    EXPECT_EQ(count_events(TextureEventKind::SNAPSHOT),
              1u);
}

TEST_F(GlFrontendTextureSnapshots, ALevelThatIsGivenAgainIsNoLongerWhatRenderingMade)
{
    m_builder.bind_texture(GL_TEXTURE_2D,
                           5);
    m_builder.enable      (GL_TEXTURE_2D);
    m_builder.copy_tex_image_2d(0,
                                GL_RGBA,
                                0,
                                0,
                                8,
                                8);
    m_builder.tex_image_2d(0,
                           8,
                           8,
                           make_pixels(256,
                                       9) );

    add_draw();

    feed();

    EXPECT_EQ(count_events(TextureEventKind::SNAPSHOT),
              0u);
}

TEST_F(GlFrontendTextureSnapshots, EachLevelThatRenderingChangedIsSnapshotOnce)
{
    m_builder.bind_texture(GL_TEXTURE_2D,
                           5);
    m_builder.enable      (GL_TEXTURE_2D);
    m_builder.copy_tex_image_2d(0,
                                GL_RGBA,
                                0,
                                0,
                                8,
                                8);
    m_builder.copy_tex_image_2d(1,
                                GL_RGBA,
                                0,
                                0,
                                4,
                                4);

    add_draw();

    feed();

    ASSERT_EQ(count_events(TextureEventKind::SNAPSHOT),
              2u);

    EXPECT_EQ(m_sink.event_vec.at(2).destination.mip_level,
              0u);
    EXPECT_EQ(m_sink.event_vec.at(3).destination.mip_level,
              1u);
    EXPECT_EQ(m_sink.event_vec.at(3).level_width,
              4u);
}
