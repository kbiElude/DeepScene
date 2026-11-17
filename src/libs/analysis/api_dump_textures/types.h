/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Texture content events of a workload, in call order, plus the upload bytes and the snapshots of rendered content (stored once per content).
 * Uses analysis vocabulary (resources, mip levels, layers), not API terms.
 */
#ifndef APIDUMPTEXTURES_TYPES_H
#define APIDUMPTEXTURES_TYPES_H

#include "analysis/api_dump_analysis/types.h"
#include "utils/graphics/texture_format.h"
#include <stdint.h>
#include <vector>

namespace Analysis
{
    namespace APIDumpTextures
    {
        /* Lossless transform applied before compression. Values are stored in files. */
        enum class BlobFilter : uint8_t
        {
            NONE            = 0,
            BYTE_PLANES_OF_4 = 1  /* planes of bytes 0, 4, 8, ..., then 1, 5, 9, ...; a remainder under 4 bytes is kept as is */
        };

        /* What an event does to a texture. Values are stored in files. */
        enum class TextureEventKind : uint8_t
        {
            UPLOAD        = 0, /* application bytes become the content of a level region (the blob)                    */
            RENDER_WRITE  = 1, /* content comes from rendering (framebuffer or target copy); no bytes unless requested */
            GENERATE_MIPS = 2, /* levels below generated_from_level are generated from that level                      */
            SNAPSHOT      = 3  /* first read of a rendered level since it changed; the blob is set after a replay      */
        };

        /* Where the bytes of a content are stored. Values are stored in files. */
        enum class BlobLocation : uint8_t
        {
            STORED         = 0, /* compressed in the file of blobs (content the workload does not hold) */
            WORKLOAD_CHUNK = 1  /* in the workload data chunk of the upload; the entry holds its size and hashes */
        };

        /* The content of an upload, byte-exact wherever stored. Hashes and size describe the bytes, so a reader can verify them. */
        struct BlobEntry
        {
            uint64_t     content_hash        = 0;
            uint32_t     data_chunk_id       = 0; /* WORKLOAD_CHUNK only: chunk number in the dump */
            uint64_t     file_offset_n_bytes = 0; /* STORED only: offset in the file of blobs */
            BlobFilter   filter              = BlobFilter::NONE; /* STORED only */
            uint64_t     length_hash         = 0; /* hash of length and bytes: contents are equal only if both hashes and the length match */
            BlobLocation location            = BlobLocation::STORED;
            uint64_t     n_raw_bytes         = 0;
            uint64_t     n_stored_bytes      = 0; /* STORED only */
        };

        /* Upload byte layout, as recorded by the front end. api_format and api_type are meaningful to that front end only.
         * Row pitch is in bytes between rows of pieces (a texel, or a block for compressed formats; see TextureFormatInfo).
         */
        struct SourceLayout
        {
            uint32_t api_format         = 0;
            uint32_t api_type           = 0;
            uint32_t row_pitch_n_bytes  = 0; /* 0 means that the rows follow each other without padding */
        };

        /* Selects every sample of a multisampled texture; the content holds the samples one after another, each as large as the region with
         * its layers. A texture that is not multisampled has only sample 0.
         */
        constexpr uint32_t ALL_SAMPLES = UINT32_MAX;
        constexpr uint32_t NO_BLOB     = UINT32_MAX;

        /* Something that happens to part of a texture. A layer is an array slice, a volume slice or a cube face (order +X, -X, +Y, -Y, +Z, -Z;
         * a cube array has six layers per cube).
         * Covers layer_count layers from destination.layer, each over the region x, y, width, height. The blob lists layers one after another,
         * row by row as the layout says.
         * Extents (region, level size) are in texels; Utils::Graphics::TextureFormats::get_n_bytes gives bytes. A level smaller than a block
         * takes one block. For compressed formats the region starts on a block and ends on a block or the level edge.
         */
        struct TextureEvent
        {
            uint32_t                                blob_index           = NO_BLOB;
            uint64_t                                call_index           = 0;
            Analysis::APIDumpAnalysis::ResourceView destination;
            uint64_t                                frame_index          = 0;
            uint32_t                                generated_from_level = 0;     /* GENERATE_MIPS only: levels below this one, in every layer */
            uint32_t                                height               = 0;
            TextureEventKind                        kind                 = TextureEventKind::UPLOAD;
            uint32_t                                layer_count          = 1;     /* counted from destination.layer; at least 1 */
            SourceLayout                            layout;

            /* The level as it is after the event: its size and format are those of the last event that names it. A volume's depth halves with
             * each level; an array has the same layer count in every level.
             */
            uint32_t                                level_api_format     = 0;     /* meaningful to the front end only */
            uint32_t                                level_depth_or_layer_count = 1;
            Utils::Graphics::TextureFormat          level_format         = Utils::Graphics::TextureFormat::UNKNOWN;
            uint32_t                                level_height         = 0;
            uint32_t                                level_width          = 0;

            /* RENDER_WRITE and SNAPSHOT of a multisampled texture: the sample, or ALL_SAMPLES. */
            uint32_t                                sample_index         = 0;
            Analysis::APIDumpAnalysis::ResourceView source;                       /* RENDER_WRITE only; no resource = the window (never stored) */
            uint32_t                                width                = 0;
            uint32_t                                x                    = 0;
            uint32_t                                y                    = 0;
        };

        /* No limit: as last frame or call, counts all events. */
        constexpr uint64_t NO_LIMIT = UINT64_MAX;

        /* Events are counted up to and including last_frame_index and last_call_index. */
        struct LevelCut
        {
            uint64_t last_call_index  = NO_LIMIT;
            uint64_t last_frame_index = NO_LIMIT;
        };

        /* A texel rectangle of a layer run of a level, provided by one event and not overwritten by a later one. */
        struct LevelPiece
        {
            uint32_t event_index = 0; /* in TextureTimeline::event_vec */
            uint32_t first_layer = 0;
            uint32_t height      = 0;
            uint32_t layer_count = 1;
            uint32_t width       = 0;
            uint32_t x           = 0;
            uint32_t y           = 0;
        };

        /* Which event made each part of a level at a point of the timeline. Uncovered parts were never written. RENDER_WRITE pieces have
         * no bytes; they mark where a snapshot has to be read.
         */
        struct LevelContent
        {
            bool                    has_level                  = false; /* an event sized the level, at or before the cut */
            bool                    has_snapshot               = false;
            uint32_t                level_depth_or_layer_count = 1;
            Utils::Graphics::TextureFormat level_format        = Utils::Graphics::TextureFormat::UNKNOWN;
            uint32_t                level_height               = 0;
            uint32_t                level_width                = 0;
            std::vector<LevelPiece> piece_vec;
            uint32_t                snapshot_event_index       = 0;     /* last SNAPSHOT of the level, at or before the cut */
        };

        /* Everything in a .textures file except the bytes of the blobs. */
        struct TextureTimeline
        {
            std::vector<BlobEntry>                           blob_vec;
            uint64_t                                         blob_file_n_bytes      = 0; /* size of the file of blobs, to tell it from another file */
            std::vector<TextureEvent>                        event_vec;
            bool                                             includes_render_content = false;
            std::vector<Analysis::APIDumpAnalysis::Resource> resource_vec;
            Analysis::APIDumpAnalysis::WorkloadReference     workload_reference;
        };
    }
}

#endif /* APIDUMPTEXTURES_TYPES_H */
