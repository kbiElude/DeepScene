/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Converts the parts of a texture timeline to and from the bytes of .textures sections (see textures_format.h). Reading validates: impossible counts, values
 * of an enumeration that does not have them, resources and blobs that are not there, calls out of order, and leftover bytes make a section invalid. Sections
 * must be read in file order, since later ones are checked against earlier ones.
 */
#ifndef APIDUMPTEXTURES_TEXTURES_SERIALIZER_H
#define APIDUMPTEXTURES_TEXTURES_SERIALIZER_H

#include "analysis/api_dump_textures/types.h"
#include "utils/binary_io/byte_reader.h"
#include "utils/binary_io/byte_writer.h"
#include <stdint.h>
#include <vector>

namespace Analysis
{
    namespace APIDumpTextures
    {
        class TexturesSerializer
        {
        public:
            static bool                 deserialize_blobs    (const std::vector<uint8_t>& in_byte_vec,
                                                              TextureTimeline*            inout_timeline_ptr);
            static bool                 deserialize_events   (const std::vector<uint8_t>& in_byte_vec,
                                                              TextureTimeline*            inout_timeline_ptr);
            static bool                 deserialize_meta     (const std::vector<uint8_t>& in_byte_vec,
                                                              TextureTimeline*            out_timeline_ptr);
            static bool                 deserialize_resources(const std::vector<uint8_t>& in_byte_vec,
                                                              TextureTimeline*            inout_timeline_ptr);
            static std::vector<uint8_t> serialize_blobs      (const TextureTimeline&      in_timeline);
            static std::vector<uint8_t> serialize_events     (const TextureTimeline&      in_timeline);
            static std::vector<uint8_t> serialize_meta       (const TextureTimeline&      in_timeline);
            static std::vector<uint8_t> serialize_resources  (const TextureTimeline&      in_timeline);

        private:
            /* Whether the values of an entry, as they were read, say what can be. */
            static bool is_entry_valid(const uint8_t&   in_filter,
                                       const uint8_t&   in_location,
                                       const BlobEntry& in_entry);

            /* Whether an event, as it was read, says what can be. */
            static bool is_event_valid(const TextureEvent&                                     in_event,
                                       const uint8_t&                                          in_kind,
                                       const uint32_t&                                         in_level_format_value,
                                       const uint64_t&                                         in_last_call_index,
                                       const size_t&                                           in_n_blobs,
                                       const std::vector<Analysis::APIDumpAnalysis::Resource>& in_resource_vec);

            /* Whether a region is on the blocks of its format. */
            static bool is_region_on_blocks(const TextureEvent& in_event);

            static Analysis::APIDumpAnalysis::ResourceView read_view(Utils::BinaryIO::ByteReader* inout_reader_ptr);

            static void write_view(Utils::BinaryIO::ByteWriter*                   inout_writer_ptr,
                                   const Analysis::APIDumpAnalysis::ResourceView& in_view);
        };
    }
}

#endif /* APIDUMPTEXTURES_TEXTURES_SERIALIZER_H */
