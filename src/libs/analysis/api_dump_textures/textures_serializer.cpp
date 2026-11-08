/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_analysis/passes_serializer.h"
#include "analysis/api_dump_textures/textures_serializer.h"
#include "utils/binary_io/byte_reader.h"
#include "utils/binary_io/byte_writer.h"

namespace
{
    /* The bytes of an entry of the table of blobs, and of an event: what is the least that a count of them can take. */
    constexpr size_t BLOB_ENTRY_N_BYTES = sizeof(uint64_t) * 5 + sizeof(uint8_t) * 2 + sizeof(uint32_t);
    constexpr size_t EVENT_N_BYTES      = sizeof(uint8_t) * 2 + sizeof(uint64_t) * 2 + sizeof(uint32_t) * 18;

    /* Whether the values of an entry, as they were read, say what can be: values of an enumeration that there are, and for a content that is left in the workload no
     * stored bytes and no filter (it has nothing of the file of blobs).
     */
    bool is_entry_valid(const uint8_t&                              in_filter,
                        const uint8_t&                              in_location,
                        const Analysis::APIDumpTextures::BlobEntry& in_entry)
    {
        const bool is_filter_known   = (in_filter   <= static_cast<uint8_t>(Analysis::APIDumpTextures::BlobFilter::BYTE_PLANES_OF_4) );
        const bool is_location_known = (in_location <= static_cast<uint8_t>(Analysis::APIDumpTextures::BlobLocation::WORKLOAD_CHUNK) );
        const bool is_in_workload    = (in_location == static_cast<uint8_t>(Analysis::APIDumpTextures::BlobLocation::WORKLOAD_CHUNK) );
        const bool has_stored_values = (in_filter                      != static_cast<uint8_t>(Analysis::APIDumpTextures::BlobFilter::NONE) ||
                                        in_entry.file_offset_n_bytes   != 0                                                                 ||
                                        in_entry.n_stored_bytes        != 0);

        return (is_filter_known   &&
                is_location_known &&
                (is_in_workload == false ||
                 has_stored_values == false) );
    }

    void write_view(Utils::BinaryIO::ByteWriter*                    inout_writer_ptr,
                    const Analysis::APIDumpAnalysis::ResourceView&  in_view)
    {
        inout_writer_ptr->write_u32(static_cast<uint32_t>(in_view.resource_id) );
        inout_writer_ptr->write_u32(in_view.mip_level);
        inout_writer_ptr->write_u32(in_view.layer);
    }

    Analysis::APIDumpAnalysis::ResourceView read_view(Utils::BinaryIO::ByteReader* inout_reader_ptr)
    {
        Analysis::APIDumpAnalysis::ResourceView result;

        result.resource_id = static_cast<Analysis::APIDumpAnalysis::ResourceId>(inout_reader_ptr->read_u32() );
        result.mip_level   = inout_reader_ptr->read_u32();
        result.layer       = inout_reader_ptr->read_u32();

        return result;
    }
}

bool Analysis::APIDumpTextures::TexturesSerializer::deserialize_blobs(const std::vector<uint8_t>& in_byte_vec,
                                                                      TextureTimeline*            inout_timeline_ptr)
{
    Utils::BinaryIO::ByteReader reader(in_byte_vec.data(),
                                       in_byte_vec.size() );
    bool                        is_valid = true;

    inout_timeline_ptr->blob_vec.clear();

    const uint32_t n_blobs = reader.read_count(BLOB_ENTRY_N_BYTES);

    for (uint32_t n_blob = 0;
                  n_blob < n_blobs && reader.has_failed() == false;
                ++n_blob)
    {
        BlobEntry entry;

        entry.content_hash        = reader.read_u64();
        entry.data_chunk_id       = reader.read_u32();
        entry.file_offset_n_bytes = reader.read_u64();

        const uint8_t filter   = reader.read_u8();
        const uint8_t location = reader.read_u8();

        entry.length_hash    = reader.read_u64();
        entry.n_raw_bytes    = reader.read_u64();
        entry.n_stored_bytes = reader.read_u64();

        if (is_entry_valid(filter,
                           location,
                           entry) )
        {
            entry.filter   = static_cast<BlobFilter>(filter);
            entry.location = static_cast<BlobLocation>(location);
        }
        else
        {
            is_valid = false;
        }

        inout_timeline_ptr->blob_vec.push_back(entry);
    }

    const bool has_failed = reader.has_failed();
    const bool is_at_end  = reader.is_at_end();

    return (is_valid           &&
            has_failed == false &&
            is_at_end);
}

bool Analysis::APIDumpTextures::TexturesSerializer::deserialize_events(const std::vector<uint8_t>& in_byte_vec,
                                                                       TextureTimeline*            inout_timeline_ptr)
{
    Utils::BinaryIO::ByteReader reader(in_byte_vec.data(),
                                       in_byte_vec.size() );
    bool                        is_valid        = true;
    uint64_t                    last_call_index = 0;

    inout_timeline_ptr->event_vec.clear();

    const uint32_t n_events    = reader.read_count(EVENT_N_BYTES);
    const size_t   n_resources = inout_timeline_ptr->resource_vec.size();
    const size_t   n_blobs     = inout_timeline_ptr->blob_vec.size();

    for (uint32_t n_event = 0;
                  n_event < n_events && reader.has_failed() == false;
                ++n_event)
    {
        TextureEvent event;

        const uint8_t kind = reader.read_u8();

        event.call_index  = reader.read_u64();
        event.frame_index = reader.read_u64();
        event.destination = read_view(&reader);
        event.x           = reader.read_u32();
        event.y           = reader.read_u32();
        event.width       = reader.read_u32();
        event.height      = reader.read_u32();
        event.blob_index  = reader.read_u32();

        event.layout.api_format        = reader.read_u32();
        event.layout.api_type          = reader.read_u32();
        event.layout.row_pitch_n_bytes = reader.read_u32();

        const uint8_t level_format_class = reader.read_u8();

        event.level_format.raw_value = reader.read_u32();
        event.level_height           = reader.read_u32();
        event.level_width            = reader.read_u32();

        event.source               = read_view(&reader);
        event.generated_from_level = reader.read_u32();

        const uint32_t destination_index = static_cast<uint32_t>(event.destination.resource_id);
        const uint32_t source_index      = static_cast<uint32_t>(event.source.resource_id);

        /* A resource is numbered from 1; a texture is something that is there; the window (no source) is what the source is when it is no resource. */
        if (level_format_class > static_cast<uint8_t>(Analysis::APIDumpAnalysis::FormatClass::UNKNOWN) ||
            kind                > static_cast<uint8_t>(TextureEventKind::SNAPSHOT) ||
            destination_index   == 0                                                   ||
            destination_index   >  n_resources                                         ||
            source_index        >  n_resources                                         ||
            event.call_index    <  last_call_index                                     ||
            (event.blob_index   != NO_BLOB && event.blob_index >= n_blobs) )
        {
            is_valid = false;
        }
        else
        {
            event.kind                      = static_cast<TextureEventKind>(kind);
            event.level_format.format_class = static_cast<Analysis::APIDumpAnalysis::FormatClass>(level_format_class);
            last_call_index                 = event.call_index;
        }

        inout_timeline_ptr->event_vec.push_back(event);
    }

    const bool has_failed = reader.has_failed();
    const bool is_at_end  = reader.is_at_end();

    return (is_valid           &&
            has_failed == false &&
            is_at_end);
}

bool Analysis::APIDumpTextures::TexturesSerializer::deserialize_meta(const std::vector<uint8_t>& in_byte_vec,
                                                                     TextureTimeline*            out_timeline_ptr)
{
    Utils::BinaryIO::ByteReader reader(in_byte_vec.data(),
                                       in_byte_vec.size() );

    *out_timeline_ptr = TextureTimeline();

    out_timeline_ptr->workload_reference.filename    = reader.read_string();
    out_timeline_ptr->workload_reference.header_hash = reader.read_u64   ();
    out_timeline_ptr->workload_reference.middle_hash = reader.read_u64   ();
    out_timeline_ptr->workload_reference.n_bytes     = reader.read_u64   ();
    out_timeline_ptr->workload_reference.tail_hash   = reader.read_u64   ();

    out_timeline_ptr->includes_render_content = reader.read_bool();
    out_timeline_ptr->blob_file_n_bytes       = reader.read_u64 ();

    const bool has_failed = reader.has_failed();
    const bool is_at_end  = reader.is_at_end();

    return (has_failed == false &&
            is_at_end);
}

bool Analysis::APIDumpTextures::TexturesSerializer::deserialize_resources(const std::vector<uint8_t>& in_byte_vec,
                                                                          TextureTimeline*            inout_timeline_ptr)
{
    Analysis::APIDumpAnalysis::Model model;
    const bool                       result = Analysis::APIDumpAnalysis::PassesSerializer::deserialize_resources(in_byte_vec,
                                                                                                               &model);

    inout_timeline_ptr->resource_vec = std::move(model.resource_vec);

    return result;
}

std::vector<uint8_t> Analysis::APIDumpTextures::TexturesSerializer::serialize_blobs(const TextureTimeline& in_timeline)
{
    Utils::BinaryIO::ByteWriter writer;

    writer.write_u32(static_cast<uint32_t>(in_timeline.blob_vec.size() ) );

    for (const auto& entry : in_timeline.blob_vec)
    {
        writer.write_u64(entry.content_hash);
        writer.write_u32(entry.data_chunk_id);
        writer.write_u64(entry.file_offset_n_bytes);
        writer.write_u8 (static_cast<uint8_t>(entry.filter) );
        writer.write_u8 (static_cast<uint8_t>(entry.location) );
        writer.write_u64(entry.length_hash);
        writer.write_u64(entry.n_raw_bytes);
        writer.write_u64(entry.n_stored_bytes);
    }

    return writer.take_bytes();
}

std::vector<uint8_t> Analysis::APIDumpTextures::TexturesSerializer::serialize_events(const TextureTimeline& in_timeline)
{
    Utils::BinaryIO::ByteWriter writer;

    writer.write_u32(static_cast<uint32_t>(in_timeline.event_vec.size() ) );

    for (const auto& event : in_timeline.event_vec)
    {
        writer.write_u8 (static_cast<uint8_t>(event.kind) );
        writer.write_u64(event.call_index);
        writer.write_u64(event.frame_index);

        write_view(&writer,
                   event.destination);

        writer.write_u32(event.x);
        writer.write_u32(event.y);
        writer.write_u32(event.width);
        writer.write_u32(event.height);
        writer.write_u32(event.blob_index);

        writer.write_u32(event.layout.api_format);
        writer.write_u32(event.layout.api_type);
        writer.write_u32(event.layout.row_pitch_n_bytes);

        writer.write_u8 (static_cast<uint8_t>(event.level_format.format_class) );
        writer.write_u32(event.level_format.raw_value);
        writer.write_u32(event.level_height);
        writer.write_u32(event.level_width);

        write_view(&writer,
                   event.source);

        writer.write_u32(event.generated_from_level);
    }

    return writer.take_bytes();
}

std::vector<uint8_t> Analysis::APIDumpTextures::TexturesSerializer::serialize_meta(const TextureTimeline& in_timeline)
{
    Utils::BinaryIO::ByteWriter writer;

    writer.write_string(in_timeline.workload_reference.filename);
    writer.write_u64   (in_timeline.workload_reference.header_hash);
    writer.write_u64   (in_timeline.workload_reference.middle_hash);
    writer.write_u64   (in_timeline.workload_reference.n_bytes);
    writer.write_u64   (in_timeline.workload_reference.tail_hash);

    writer.write_bool(in_timeline.includes_render_content);
    writer.write_u64 (in_timeline.blob_file_n_bytes);

    return writer.take_bytes();
}

std::vector<uint8_t> Analysis::APIDumpTextures::TexturesSerializer::serialize_resources(const TextureTimeline& in_timeline)
{
    Analysis::APIDumpAnalysis::Model model;

    model.resource_vec = in_timeline.resource_vec;

    return Analysis::APIDumpAnalysis::PassesSerializer::serialize_resources(model);
}
