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

            resource.description.height          = 64;
            resource.description.kind            = Analysis::APIDumpAnalysis::ResourceKind::TEXTURE_2D;
            resource.description.mip_level_count = 3;
            resource.description.width           = 128;
            resource.id                          = static_cast<Analysis::APIDumpAnalysis::ResourceId>(n_resource);

            result.resource_vec.push_back(resource);
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
        upload.level_format.format_class = Analysis::APIDumpAnalysis::FormatClass::COLOR;
        upload.level_format.raw_value    = 0x8058;
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

        result.event_vec = {upload, render_write, generate, snapshot};

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
              4u);
    ASSERT_EQ(loaded.blob_vec.size(),
              3u);

    /* The level as the event leaves it. */
    EXPECT_EQ(loaded.event_vec.at(0).level_width,
              128u);
    EXPECT_EQ(loaded.event_vec.at(0).level_height,
              64u);
    EXPECT_EQ(loaded.event_vec.at(0).level_format.raw_value,
              0x8058u);
    EXPECT_EQ(loaded.event_vec.at(0).level_format.format_class,
              Analysis::APIDumpAnalysis::FormatClass::COLOR);

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
    again.call_index                = 50;
    again.destination               = make_view(1, 0, 0);
    again.frame_index               = 2;
    again.height                    = 32;
    again.kind                      = TextureEventKind::UPLOAD;
    again.level_format.format_class = Analysis::APIDumpAnalysis::FormatClass::COMPRESSED;
    again.level_format.raw_value    = 0x83F1;
    again.level_height              = 32;
    again.level_width               = 64;
    again.width                     = 64;

    timeline.event_vec.push_back(again);

    ASSERT_TRUE(TexturesFile::read_bytes(TexturesFile::write_bytes(timeline),
                                        &loaded,
                                        &error) ) << error;

    ASSERT_EQ(loaded.event_vec.size(),
              5u);

    /* What the first event said of the level is not what the last says: both are kept, in the order of the calls. */
    EXPECT_EQ(loaded.event_vec.at(0).level_width,
              128u);
    EXPECT_EQ(loaded.event_vec.at(4).level_width,
              64u);
    EXPECT_EQ(loaded.event_vec.at(4).level_format.format_class,
              Analysis::APIDumpAnalysis::FormatClass::COMPRESSED);
    EXPECT_EQ(loaded.event_vec.at(4).blob_index,
              Analysis::APIDumpTextures::NO_BLOB);
}
