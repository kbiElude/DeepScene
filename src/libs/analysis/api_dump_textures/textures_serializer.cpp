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
    constexpr size_t EVENT_N_BYTES      = sizeof(uint8_t) + sizeof(uint64_t) * 2 + sizeof(uint32_t) * 22;

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

    /* Whether a region is on the blocks of its format: it starts on a block, and ends on one or at the edge of the level, in each direction. A format of blocks of
     * one texel has every region on them.
     */
    bool is_region_on_blocks(const Analysis::APIDumpTextures::TextureEvent& in_event)
    {
        const Utils::Graphics::TextureFormatInfo block = Utils::Graphics::TextureFormats::get_info(in_event.level_format);

        const bool is_x_ok = (in_event.x % block.block_width_in_texels == 0 &&
                              (in_event.width % block.block_width_in_texels == 0 ||
                               static_cast<uint64_t>(in_event.x) + in_event.width == in_event.level_width) );
        const bool is_y_ok = (in_event.y % block.block_height_in_texels == 0 &&
                              (in_event.height % block.block_height_in_texels == 0 ||
                               static_cast<uint64_t>(in_event.y) + in_event.height == in_event.level_height) );

        return (is_x_ok &&
                is_y_ok);
    }

    /* Whether an event, as it was read, says what can be: values of the enumerations that there are, resources that are there (they are numbered from 1; the
     * window, which is no resource, may be the source), calls in order, a blob that is in the table, at least one layer, for a multisampled texture a sample
     * that it has, and a format that there is with a region on its blocks.
     */
    bool is_event_valid(const Analysis::APIDumpTextures::TextureEvent&              in_event,
                        const uint8_t&                                              in_kind,
                        const uint32_t&                                             in_level_format_value,
                        const uint64_t&                                             in_last_call_index,
                        const size_t&                                               in_n_blobs,
                        const std::vector<Analysis::APIDumpAnalysis::Resource>&     in_resource_vec)
    {
        const size_t   n_resources       = in_resource_vec.size();
        const uint32_t destination_index = static_cast<uint32_t>(in_event.destination.resource_id);
        const uint32_t source_index      = static_cast<uint32_t>(in_event.source.resource_id);
        const bool     are_resources_ok  = (destination_index != 0            &&
                                            destination_index <= n_resources  &&
                                            source_index      <= n_resources);
        bool           is_sample_ok      = false;

        if (are_resources_ok)
        {
            const uint32_t sample_count = in_resource_vec.at(destination_index - 1).description.sample_count;

            is_sample_ok = (in_event.sample_index == Analysis::APIDumpTextures::ALL_SAMPLES ||
                            in_event.sample_index <  (sample_count > 0 ? sample_count : 1) );
        }

        const bool are_enums_ok  = (Utils::Graphics::TextureFormats::is_format_value(in_level_format_value) &&
                                    in_kind <= static_cast<uint8_t>(Analysis::APIDumpTextures::TextureEventKind::SNAPSHOT) );
        const bool is_blob_ok    = (in_event.blob_index == Analysis::APIDumpTextures::NO_BLOB ||
                                    in_event.blob_index <  in_n_blobs);
        const bool are_layers_ok = (in_event.layer_count                >= 1                                                       &&
                                    in_event.level_depth_or_layer_count >= 1                                                       &&
                                    in_event.layer_count                <= UINT32_MAX - in_event.destination.layer);

        const bool is_region_ok = is_region_on_blocks(in_event);

        return (are_enums_ok               &&
                are_layers_ok              &&
                are_resources_ok           &&
                is_blob_ok                 &&
                is_region_ok               &&
                is_sample_ok               &&
                in_event.call_index >= in_last_call_index);
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

    const uint32_t n_events = reader.read_count(EVENT_N_BYTES);
    const size_t   n_blobs  = inout_timeline_ptr->blob_vec.size();

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

        const uint32_t level_format_value = reader.read_u32();

        event.level_api_format = reader.read_u32();
        event.level_height     = reader.read_u32();
        event.level_width      = reader.read_u32();

        event.source               = read_view(&reader);
        event.generated_from_level = reader.read_u32();

        event.layer_count                = reader.read_u32();
        event.level_depth_or_layer_count = reader.read_u32();
        event.sample_index               = reader.read_u32();

        /* A value that is not a format is left as the UNKNOWN, and the event is not valid. */
        if (Utils::Graphics::TextureFormats::is_format_value(level_format_value) )
        {
            event.level_format = static_cast<Utils::Graphics::TextureFormat>(level_format_value);
        }

        if (is_event_valid(event,
                           kind,
                           level_format_value,
                           last_call_index,
                           n_blobs,
                           inout_timeline_ptr->resource_vec) )
        {
            event.kind      = static_cast<TextureEventKind>(kind);
            last_call_index = event.call_index;
        }
        else
        {
            is_valid = false;
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

        writer.write_u32(static_cast<uint32_t>(event.level_format) );
        writer.write_u32(event.level_api_format);
        writer.write_u32(event.level_height);
        writer.write_u32(event.level_width);

        write_view(&writer,
                   event.source);

        writer.write_u32(event.generated_from_level);
        writer.write_u32(event.layer_count);
        writer.write_u32(event.level_depth_or_layer_count);
        writer.write_u32(event.sample_index);
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
