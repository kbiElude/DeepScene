/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Tests for the .textures file: what is written is read back, the same timeline gives the same bytes, and a file that is damaged or that says what is not there
 * is refused.
 */
#include <gtest/gtest.h>
#include "analysis/api_dump_textures/textures_file.h"
#include "temp_directory.h"

namespace
{
    typedef Analysis::APIDumpTextures::TextureEvent     TextureEvent;
    typedef Analysis::APIDumpTextures::TextureEventKind TextureEventKind;
    typedef Analysis::APIDumpTextures::TexturesFile     TexturesFile;
    typedef Analysis::APIDumpTextures::TextureTimeline  TextureTimeline;

    Analysis::APIDumpAnalysis::ResourceView make_view(const uint32_t& in_resource,
                                                      const uint32_t& in_mip_level,
                                                      const uint32_t& in_layer)
    {
        Analysis::APIDumpAnalysis::ResourceView result;

        result.layer       = in_layer;
        result.mip_level   = in_mip_level;
        result.resource_id = static_cast<Analysis::APIDumpAnalysis::ResourceId>(in_resource);

        return result;
    }

    /* Two textures, two contents, and an event of each kind. */
    TextureTimeline make_timeline()
    {
        TextureTimeline result;

        for (uint32_t n_resource = 1; n_resource <= 2; ++n_resource)
        {
            Analysis::APIDumpAnalysis::Resource resource;

            /* The first is an array of six layers. */
            resource.description.depth_or_layer_count = (n_resource == 1) ? 6 : 1;
            resource.description.height          = 64;
            resource.description.kind            = Analysis::APIDumpAnalysis::ResourceKind::TEXTURE_2D;
            resource.description.mip_level_count = 3;
            resource.description.width           = 128;
            resource.id                          = static_cast<Analysis::APIDumpAnalysis::ResourceId>(n_resource);

            result.resource_vec.push_back(resource);
        }

        /* A multisampled texture, which has its content from rendering alone. */
        {
            Analysis::APIDumpAnalysis::Resource multisampled;

            multisampled.description.height          = 64;
            multisampled.description.kind            = Analysis::APIDumpAnalysis::ResourceKind::TEXTURE_2D;
            multisampled.description.mip_level_count = 1;
            multisampled.description.sample_count    = 4;
            multisampled.description.width           = 128;
            multisampled.id                          = static_cast<Analysis::APIDumpAnalysis::ResourceId>(3);

            result.resource_vec.push_back(multisampled);
        }

        for (uint64_t n_blob = 0; n_blob < 2; ++n_blob)
        {
            Analysis::APIDumpTextures::BlobEntry entry;

            entry.content_hash        = 0x1000 + n_blob;
            entry.file_offset_n_bytes = 16 + n_blob * 100;
            entry.filter              = (n_blob == 0) ? Analysis::APIDumpTextures::BlobFilter::NONE
                                                      : Analysis::APIDumpTextures::BlobFilter::BYTE_PLANES_OF_4;
            entry.length_hash         = 0x2000 + n_blob;
            entry.n_raw_bytes         = 256;
            entry.n_stored_bytes      = 100;

            result.blob_vec.push_back(entry);
        }

        /* An upload that is left in the workload: its data chunk, with the size and the hashes of the bytes. */
        Analysis::APIDumpTextures::BlobEntry workload_entry;

        workload_entry.content_hash  = 0x3000;
        workload_entry.data_chunk_id = 77;
        workload_entry.length_hash   = 0x4000;
        workload_entry.location      = Analysis::APIDumpTextures::BlobLocation::WORKLOAD_CHUNK;
        workload_entry.n_raw_bytes   = 512;

        result.blob_vec.push_back(workload_entry);

        TextureEvent upload;
        TextureEvent render_write;
        TextureEvent generate;
        TextureEvent snapshot;

        upload.blob_index               = 2;
        upload.level_api_format          = 0x8058;
        upload.level_format              = Utils::Graphics::TextureFormat::R8G8B8A8_UNORM;
        upload.level_height              = 64;
        upload.level_width               = 128;
        upload.call_index               = 10;
        upload.destination              = make_view(1, 0, 0);
        upload.frame_index              = 0;
        upload.height                   = 64;
        upload.kind                     = TextureEventKind::UPLOAD;
        upload.layout.api_format        = 0x1908;
        upload.layout.api_type          = 0x1401;
        upload.layout.row_pitch_n_bytes = 512;
        upload.width                    = 128;

        render_write.call_index  = 20;
        render_write.destination = make_view(2, 1, 0);
        render_write.frame_index = 1;
        render_write.height      = 32;
        render_write.kind        = TextureEventKind::RENDER_WRITE;
        render_write.width       = 64;
        render_write.x           = 4;
        render_write.y           = 8;

        generate.call_index           = 30;
        generate.destination          = make_view(1, 1, 0);
        generate.frame_index          = 1;
        generate.generated_from_level = 0;
        generate.kind                 = TextureEventKind::GENERATE_MIPS;

        /* The content that rendering made, as it was when it was first read: stored. */
        snapshot.blob_index  = 1;
        snapshot.call_index  = 40;
        snapshot.destination = make_view(2, 1, 0);
        snapshot.frame_index = 1;
        snapshot.height      = 32;
        snapshot.kind        = TextureEventKind::SNAPSHOT;
        snapshot.width       = 64;
        snapshot.x           = 4;
        snapshot.y           = 8;

        /* Three layers of the array in one upload, and every sample of the multisampled texture as it was when it was read. */
        TextureEvent layered_upload;
        TextureEvent multisampled_snapshot;

        layered_upload.blob_index                 = 2;
        layered_upload.call_index                 = 60;
        layered_upload.destination                = make_view(1, 0, 2);
        layered_upload.frame_index                = 2;
        layered_upload.height                     = 64;
        layered_upload.kind                       = TextureEventKind::UPLOAD;
        layered_upload.layer_count                = 3;
        layered_upload.level_depth_or_layer_count = 6;
        layered_upload.level_height               = 64;
        layered_upload.level_width                = 128;
        layered_upload.width                      = 128;

        multisampled_snapshot.blob_index                 = 1;
        multisampled_snapshot.call_index                 = 70;
        multisampled_snapshot.destination                = make_view(3, 0, 0);
        multisampled_snapshot.frame_index                = 2;
        multisampled_snapshot.height                     = 64;
        multisampled_snapshot.kind                       = TextureEventKind::SNAPSHOT;
        multisampled_snapshot.level_height               = 64;
        multisampled_snapshot.level_width                = 128;
        multisampled_snapshot.sample_index               = Analysis::APIDumpTextures::ALL_SAMPLES;
        multisampled_snapshot.width                      = 128;

        result.event_vec = {upload, render_write, generate, snapshot, layered_upload, multisampled_snapshot};

        result.blob_file_n_bytes       = 216;
        result.includes_render_content = true;

        result.workload_reference.filename    = "capture.workload";
        result.workload_reference.header_hash = 1;
        result.workload_reference.middle_hash = 2;
        result.workload_reference.n_bytes     = 3;
        result.workload_reference.tail_hash   = 4;

        return result;
    }
}

TEST(TexturesFile, ATimelineComesBackAsItWasWritten)
{
    const auto      timeline = make_timeline();
    TextureTimeline loaded;
    std::string     error;

    ASSERT_TRUE(TexturesFile::read_bytes(TexturesFile::write_bytes(timeline),
                                        &loaded,
                                        &error) ) << error;

    EXPECT_EQ(TexturesFile::write_bytes(loaded),
              TexturesFile::write_bytes(timeline) );

    ASSERT_EQ(loaded.event_vec.size(),
              6u);
    ASSERT_EQ(loaded.blob_vec.size(),
              3u);

    /* The layers of the upload, the level that has them, and the samples of the snapshot. */
    EXPECT_EQ(loaded.event_vec.at(4).destination.layer,
              2u);
    EXPECT_EQ(loaded.event_vec.at(4).layer_count,
              3u);
    EXPECT_EQ(loaded.event_vec.at(4).level_depth_or_layer_count,
              6u);
    EXPECT_EQ(loaded.event_vec.at(5).sample_index,
              Analysis::APIDumpTextures::ALL_SAMPLES);
    EXPECT_EQ(loaded.event_vec.at(0).layer_count,
              1u);

    /* The level as the event leaves it. */
    EXPECT_EQ(loaded.event_vec.at(0).level_width,
              128u);
    EXPECT_EQ(loaded.event_vec.at(0).level_height,
              64u);
    EXPECT_EQ(loaded.event_vec.at(0).level_api_format,
              0x8058u);
    EXPECT_EQ(loaded.event_vec.at(0).level_format,
              Utils::Graphics::TextureFormat::R8G8B8A8_UNORM);

    EXPECT_EQ(loaded.event_vec.at(0).layout.row_pitch_n_bytes,
              512u);
    EXPECT_EQ(loaded.event_vec.at(1).kind,
              TextureEventKind::RENDER_WRITE);
    EXPECT_EQ(loaded.event_vec.at(1).blob_index,
              Analysis::APIDumpTextures::NO_BLOB);
    EXPECT_EQ(loaded.blob_vec.at(1).filter,
              Analysis::APIDumpTextures::BlobFilter::BYTE_PLANES_OF_4);
    EXPECT_EQ(loaded.blob_vec.at(2).location,
              Analysis::APIDumpTextures::BlobLocation::WORKLOAD_CHUNK);
    EXPECT_EQ(loaded.blob_vec.at(2).data_chunk_id,
              77u);
    EXPECT_EQ(loaded.event_vec.at(3).kind,
              TextureEventKind::SNAPSHOT);
    EXPECT_TRUE(loaded.includes_render_content);
    EXPECT_EQ(loaded.blob_file_n_bytes,
              216u);
    EXPECT_EQ(loaded.workload_reference.filename,
              "capture.workload");
}

TEST(TexturesFile, AFileThatIsWrittenToDiskIsReadBackAndHasTheNameOfItsBlobsBesideIt)
{
    Tests::TempDirectory directory;
    const auto           timeline = make_timeline();
    const std::string    filename = directory.get_path() + "\\capture.textures";
    TextureTimeline      loaded;
    std::string          error;

    ASSERT_TRUE(TexturesFile::write(filename,
                                    timeline,
                                   &error) ) << error;
    ASSERT_TRUE(TexturesFile::read (filename,
                                   &loaded,
                                   &error) ) << error;

    EXPECT_EQ(TexturesFile::write_bytes(loaded),
              TexturesFile::write_bytes(timeline) );
    EXPECT_EQ(TexturesFile::get_blob_filename(filename),
              filename + ".data");
}

TEST(TexturesFile, ABitThatIsFlippedAnywhereInTheFileIsNoticedOrChangesNothing)
{
    const auto           bytes = TexturesFile::write_bytes(make_timeline() );
    TextureTimeline      loaded;
    std::string          error;

    /* A bit that the decompressor does not use (one that a frame header leaves free) may be flipped without a change in what is read: that is no damage. */
    for (size_t n_byte = 0; n_byte < bytes.size(); ++n_byte)
    {
        auto damaged_byte_vec = bytes;

        damaged_byte_vec.at(n_byte) ^= 0x10;

        if (TexturesFile::read_bytes(damaged_byte_vec,
                                    &loaded,
                                    &error) )
        {
            EXPECT_EQ(TexturesFile::write_bytes(loaded),
                      bytes) << "byte " << n_byte;
        }
    }
}

TEST(TexturesFile, AFileThatIsCutIsRefused)
{
    const auto      bytes = TexturesFile::write_bytes(make_timeline() );
    TextureTimeline loaded;
    std::string     error;

    for (const size_t n_bytes : {size_t(0), size_t(10), bytes.size() / 2, bytes.size() - 1})
    {
        const std::vector<uint8_t> cut_byte_vec(bytes.begin(),
                                                bytes.begin() + n_bytes);

        EXPECT_FALSE(TexturesFile::read_bytes(cut_byte_vec,
                                             &loaded,
                                             &error) ) << n_bytes << " bytes";
    }
}

TEST(TexturesFile, AnEventThatNamesWhatIsNotThereIsRefused)
{
    TextureTimeline loaded;
    std::string     error;

    /* A blob that the table does not have. */
    {
        auto timeline = make_timeline();

        timeline.event_vec.at(0).blob_index = 7;

        EXPECT_FALSE(TexturesFile::read_bytes(TexturesFile::write_bytes(timeline),
                                             &loaded,
                                             &error) );
    }

    /* A texture that is not one of the resources, and the no resource as a destination. */
    {
        auto timeline = make_timeline();

        timeline.event_vec.at(0).destination = make_view(9, 0, 0);

        EXPECT_FALSE(TexturesFile::read_bytes(TexturesFile::write_bytes(timeline),
                                             &loaded,
                                             &error) );

        timeline.event_vec.at(0).destination = make_view(0, 0, 0);

        EXPECT_FALSE(TexturesFile::read_bytes(TexturesFile::write_bytes(timeline),
                                             &loaded,
                                             &error) );
    }

    /* A kind that does not exist, and calls out of order. */
    {
        auto timeline = make_timeline();

        timeline.event_vec.at(0).kind = static_cast<TextureEventKind>(9);

        EXPECT_FALSE(TexturesFile::read_bytes(TexturesFile::write_bytes(timeline),
                                             &loaded,
                                             &error) );
    }

    {
        auto timeline = make_timeline();

        timeline.event_vec.at(2).call_index = 5;

        EXPECT_FALSE(TexturesFile::read_bytes(TexturesFile::write_bytes(timeline),
                                             &loaded,
                                             &error) );
    }
}

TEST(TexturesFile, AContentLeftInTheWorkloadThatHasStoredValuesIsRefused)
{
    TextureTimeline loaded;
    std::string     error;

    /* It has nothing of the file of blobs: no place in it, no stored bytes, and no filter. */
    {
        auto timeline = make_timeline();

        timeline.blob_vec.at(2).n_stored_bytes = 10;

        EXPECT_FALSE(TexturesFile::read_bytes(TexturesFile::write_bytes(timeline),
                                             &loaded,
                                             &error) );
    }

    {
        auto timeline = make_timeline();

        timeline.blob_vec.at(2).filter = Analysis::APIDumpTextures::BlobFilter::BYTE_PLANES_OF_4;

        EXPECT_FALSE(TexturesFile::read_bytes(TexturesFile::write_bytes(timeline),
                                             &loaded,
                                             &error) );
    }

    {
        auto timeline = make_timeline();

        timeline.blob_vec.at(2).location = static_cast<Analysis::APIDumpTextures::BlobLocation>(5);

        EXPECT_FALSE(TexturesFile::read_bytes(TexturesFile::write_bytes(timeline),
                                             &loaded,
                                             &error) );
    }
}

TEST(TexturesFile, ALevelThatIsSpecifiedAgainKeepsTheSizeAndFormatOfEachTimeInItsEvents)
{
    auto            timeline = make_timeline();
    TextureEvent    again;
    TextureTimeline loaded;
    std::string     error;

    /* The level of the first event is made again, smaller and of another format. */
    again.blob_index                = Analysis::APIDumpTextures::NO_BLOB;
    again.call_index                = 100;
    again.destination               = make_view(1, 0, 0);
    again.frame_index               = 2;
    again.height                    = 32;
    again.kind                      = TextureEventKind::UPLOAD;
    again.level_api_format          = 0x83F1;
    again.level_format              = Utils::Graphics::TextureFormat::BC1_UNORM;
    again.level_height              = 32;
    again.level_width               = 64;
    again.width                     = 64;

    timeline.event_vec.push_back(again);

    ASSERT_TRUE(TexturesFile::read_bytes(TexturesFile::write_bytes(timeline),
                                        &loaded,
                                        &error) ) << error;

    ASSERT_EQ(loaded.event_vec.size(),
              7u);

    /* What the first event said of the level is not what the last says: both are kept, in the order of the calls. */
    EXPECT_EQ(loaded.event_vec.at(0).level_width,
              128u);
    EXPECT_EQ(loaded.event_vec.at(6).level_width,
              64u);
    EXPECT_EQ(loaded.event_vec.at(6).level_format,
              Utils::Graphics::TextureFormat::BC1_UNORM);
    EXPECT_EQ(loaded.event_vec.at(6).blob_index,
              Analysis::APIDumpTextures::NO_BLOB);
}
namespace
{
    /* An upload to the array texture that comes after every event of the sample timeline, of the format and the region that the test says. */
    TextureEvent make_extra_event(const Utils::Graphics::TextureFormat& in_format,
                                  const uint32_t&                       in_level_width,
                                  const uint32_t&                       in_level_height,
                                  const uint32_t&                       in_x,
                                  const uint32_t&                       in_y,
                                  const uint32_t&                       in_width,
                                  const uint32_t&                       in_height)
    {
        TextureEvent result;

        result.call_index   = 200;
        result.destination  = make_view(1, 0, 0);
        result.frame_index  = 3;
        result.height       = in_height;
        result.kind         = TextureEventKind::UPLOAD;
        result.level_format = in_format;
        result.level_height = in_level_height;
        result.level_width  = in_level_width;
        result.width        = in_width;
        result.x            = in_x;
        result.y            = in_y;

        return result;
    }

    bool is_read_back(const TextureEvent& in_event)
    {
        auto            timeline = make_timeline();
        TextureTimeline loaded;
        std::string     error;

        timeline.event_vec.push_back(in_event);

        return TexturesFile::read_bytes(TexturesFile::write_bytes(timeline),
                                       &loaded,
                                       &error);
    }
}

TEST(TexturesFile, TheRegionOfACompressedTextureIsInTexelsAndOnItsBlocks)
{
    const auto bc1 = Utils::Graphics::TextureFormat::BC1_UNORM;

    /* A region of whole blocks inside the level. */
    EXPECT_TRUE(is_read_back(make_extra_event(bc1, 128, 64, 4, 8, 64, 32) ) );

    /* A level that is smaller than a block, and one that is not a multiple of it: the region that runs to the edge of the level is on the blocks. */
    EXPECT_TRUE(is_read_back(make_extra_event(bc1, 2, 2, 0, 0, 2, 2) ) );
    EXPECT_TRUE(is_read_back(make_extra_event(bc1, 6, 6, 0, 0, 6, 6) ) );
    EXPECT_TRUE(is_read_back(make_extra_event(bc1, 6, 6, 4, 4, 2, 2) ) );

    /* A format of one texel for a block has every region on its blocks. */
    EXPECT_TRUE(is_read_back(make_extra_event(Utils::Graphics::TextureFormat::R8G8B8A8_UNORM, 128, 64, 3, 5, 7, 9) ) );
}

TEST(TexturesFile, ARegionThatIsNotOnTheBlocksOfACompressedTextureIsRefused)
{
    const auto bc3 = Utils::Graphics::TextureFormat::BC3_UNORM;

    /* It starts inside a block. */
    EXPECT_FALSE(is_read_back(make_extra_event(bc3, 128, 64, 2, 0, 8, 8) ) );
    EXPECT_FALSE(is_read_back(make_extra_event(bc3, 128, 64, 0, 2, 8, 8) ) );

    /* It ends inside a block that is not the last of the level. */
    EXPECT_FALSE(is_read_back(make_extra_event(bc3, 128, 64, 0, 0, 6, 8) ) );
    EXPECT_FALSE(is_read_back(make_extra_event(bc3, 128, 64, 0, 0, 8, 6) ) );
}

TEST(TexturesFile, AValueThatIsNotAFormatIsRefused)
{
    auto event = make_extra_event(Utils::Graphics::TextureFormat::R8G8B8A8_UNORM, 128, 64, 0, 0, 128, 64);

    EXPECT_TRUE(is_read_back(event) );

    event.level_format = static_cast<Utils::Graphics::TextureFormat>(Utils::Graphics::LAST_TEXTURE_FORMAT_VALUE + 1);

    EXPECT_FALSE(is_read_back(event) );
}

TEST(TexturesFile, TheLayersAndTheSamplesOfAnEventAreThoseThatTheTextureCanHave)
{
    /* The layers: one at least, and not so many that they run past the number there is to count. */
    {
        auto event = make_extra_event(Utils::Graphics::TextureFormat::R8G8B8A8_UNORM, 128, 64, 0, 0, 128, 64);

        EXPECT_TRUE(is_read_back(event) );

        event.layer_count = 0;

        EXPECT_FALSE(is_read_back(event) );

        event.destination = make_view(1, 0, 5);
        event.layer_count = UINT32_MAX;

        EXPECT_FALSE(is_read_back(event) );

        event.layer_count                = 3;
        event.level_depth_or_layer_count = 0;

        EXPECT_FALSE(is_read_back(event) );
    }

    /* The samples: those of the texture (the multisampled one has four), or all of them; the one that has no more than the one sample has the sample 0. */
    {
        auto event = make_extra_event(Utils::Graphics::TextureFormat::R8G8B8A8_UNORM, 128, 64, 0, 0, 128, 64);

        event.destination = make_view(3, 0, 0);
        event.kind        = TextureEventKind::SNAPSHOT;

        event.sample_index = 3;

        EXPECT_TRUE(is_read_back(event) );

        event.sample_index = Analysis::APIDumpTextures::ALL_SAMPLES;

        EXPECT_TRUE(is_read_back(event) );

        event.sample_index = 4;

        EXPECT_FALSE(is_read_back(event) );

        event.destination  = make_view(2, 0, 0);
        event.sample_index = 1;

        EXPECT_FALSE(is_read_back(event) );
    }
}

TEST(TexturesFile, AnUploadWhosePixelsWereNotCapturedComesBackAsSuchAndIsNotTakenForOneWithNoData)
{
    auto            timeline = make_timeline();
    auto            event    = make_extra_event(Utils::Graphics::TextureFormat::R8G8B8A8_UNORM, 128, 64, 0, 0, 128, 64);
    TextureTimeline loaded;
    std::string     error;

    event.is_content_not_captured = true;

    timeline.event_vec.push_back(event);

    ASSERT_TRUE(TexturesFile::read_bytes(TexturesFile::write_bytes(timeline),
                                        &loaded,
                                        &error) ) << error;

    EXPECT_TRUE (loaded.event_vec.back().is_content_not_captured);
    EXPECT_FALSE(loaded.event_vec.front().is_content_not_captured);
}

TEST(TexturesFile, ContentThatWasNotCapturedIsOnlyOfAnUploadThatHasNoBlob)
{
    auto event = make_extra_event(Utils::Graphics::TextureFormat::R8G8B8A8_UNORM, 128, 64, 0, 0, 128, 64);

    event.is_content_not_captured = true;

    EXPECT_TRUE(is_read_back(event) );

    /* What is not an upload has no pixels that were given. */
    event.kind = TextureEventKind::SNAPSHOT;

    EXPECT_FALSE(is_read_back(event) );

    /* An upload that points at a content has it. */
    event.kind       = TextureEventKind::UPLOAD;
    event.blob_index = 0;

    EXPECT_FALSE(is_read_back(event) );
}
