/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * What the textures of a workload hold, and when: the events that change the content of a texture, in the order of the calls, and the bytes that the
 * uploads carried (left where they are in the workload), and the snapshots of what rendering made (stored once for each content, whatever the number of times it is read). The vocabulary is that of the analysis (resources, mip levels,
 * layers), not of any API.
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
        /* A transform applied to the bytes of a content before they are compressed. Neither transform loses information. The numeric values are
         * stored in files, so they must not change.
         */
        enum class BlobFilter : uint8_t
        {
            NONE            = 0,
            BYTE_PLANES_OF_4 = 1  /* the bytes are split into planes: bytes 0, 4, 8, ..., then 1, 5, 9, ...; a remainder of fewer than 4 bytes stays as it is */
        };

        /* What an event does to a texture. The numeric values are stored in files, so they must not change. */
        enum class TextureEventKind : uint8_t
        {
            UPLOAD        = 0, /* bytes from the application become the content of a region of a level (the blob)                                                     */
            RENDER_WRITE  = 1, /* the content comes from rendering (a copy from a framebuffer or a render target); no bytes unless requested                          */
            GENERATE_MIPS = 2, /* the driver generates the levels below generated_from_level from that level                                                          */
            SNAPSHOT      = 3  /* a level that rendering changed is read for the first time since; holds the content then (the blob, set once a replay reads it back) */
        };

        /* Where the bytes of a content are stored. The numeric values are stored in files, so they must not change. */
        enum class BlobLocation : uint8_t
        {
            STORED         = 0, /* in the file of blobs, compressed (for content that the workload does not hold)                                                     */
            WORKLOAD_CHUNK = 1  /* in the data chunk of the workload that the upload carried; nothing is copied, and the entry holds the size and hashes of the chunk */
        };

        /* The content that an upload carried, exactly as it was given: the bytes are never changed, wherever they are stored. The hashes and the size
         * describe the bytes themselves, so that a reader can verify that what it got from the file of blobs or from the workload is the content.
         */
        struct BlobEntry
        {
            uint64_t     content_hash        = 0;
            uint32_t     data_chunk_id       = 0; /* WORKLOAD_CHUNK only: the data chunk of the workload, by its number in the dump */
            uint64_t     file_offset_n_bytes = 0; /* STORED only: the offset of the compressed bytes in the file of blobs */
            BlobFilter   filter              = BlobFilter::NONE; /* STORED only */
            uint64_t     length_hash         = 0; /* a second hash, of the length and the bytes: two contents are equal only if both hashes and the length match */
            BlobLocation location            = BlobLocation::STORED;
            uint64_t     n_raw_bytes         = 0;
            uint64_t     n_stored_bytes      = 0; /* STORED only */
        };

        /* How the bytes of an upload are laid out, as described by the front end that recorded the upload. api_format and api_type mean something
         * only to that front end (like Format::raw_value). The exception is the row pitch, the distance in bytes from one row of pieces to the next.
         * A piece is a texel for an uncompressed format and a block for a compressed one (see TextureFormatInfo).
         */
        struct SourceLayout
        {
            uint32_t api_format         = 0;
            uint32_t api_type           = 0;
            uint32_t row_pitch_n_bytes  = 0; /* 0 means that the rows follow each other without padding */
        };

        /* Selects every sample of a multisampled texture. Otherwise a sample is selected by its index. A content with this value holds the samples
         * one after another, the first sample first. Each sample is as large as the region, including its layers. A texture that is not multisampled
         * has only sample 0.
         */
        constexpr uint32_t ALL_SAMPLES = UINT32_MAX;
        constexpr uint32_t NO_BLOB     = UINT32_MAX;

        /* Something that happens to part of a texture. Layers are defined as in the analysis: a layer is a slice of a texture array, a slice of a
         * volume (3D), or a face of a cube. Cube faces come in the order +X, -X, +Y, -Y, +Z, -Z, and a cube array has six layers per cube.
         *
         * The event covers layer_count layers, starting at destination.layer. Each layer is covered over the region x, y, width, height. The content
         * of the blob lists the layers one after another, each one row by row, as the layout says.
         *
         * All extents of the event (the region and the size of the level) are in texels, whatever the format. The block size of the format turns
         * them into bytes (see Utils::Graphics::TextureFormats::get_n_bytes). A level smaller than a block, such as the last levels of a compressed
         * texture, takes one block. For a compressed format the region starts on a block (x and y are multiples of the block size) and ends on a
         * block, or at the edge of the level.
         */
        struct TextureEvent
        {
            uint32_t                                blob_index           = NO_BLOB;
            uint64_t                                call_index           = 0;
            Analysis::APIDumpAnalysis::ResourceView destination;
            uint64_t                                frame_index          = 0;
            uint32_t                                generated_from_level = 0;     /* GENERATE_MIPS only: the event generates the levels below this one, in every layer */
            uint32_t                                height               = 0;
            TextureEventKind                        kind                 = TextureEventKind::UPLOAD;
            uint32_t                                layer_count          = 1;     /* counted from destination.layer; at least 1 */
            SourceLayout                            layout;

            /* The level as it is after the event. The size and format of a level at any time are therefore those of the last event that names it.
             * An upload of the whole level can give it a different size or format than it had (a texture that is specified again), and the level is
             * then as the event says. For a volume, the depth is the depth of the level, which halves with each level. An array has the same number
             * of layers in every level.
             */
            uint32_t                                level_api_format     = 0;     /* the API format value; meaningful to its front end only */
            uint32_t                                level_depth_or_layer_count = 1;
            Utils::Graphics::TextureFormat          level_format         = Utils::Graphics::TextureFormat::UNKNOWN;
            uint32_t                                level_height         = 0;
            uint32_t                                level_width          = 0;

            /* RENDER_WRITE and SNAPSHOT of a multisampled texture: the sample that the event refers to, or ALL_SAMPLES. A multisampled texture
             * cannot be uploaded to, so its content comes only from rendering.
             */
            uint32_t                                sample_index         = 0;
            Analysis::APIDumpAnalysis::ResourceView source;                       /* RENDER_WRITE only; no resource = the window (never stored) */
            uint32_t                                width                = 0;
            uint32_t                                x                    = 0;
            uint32_t                                y                    = 0;
        };

        /* No limit. It is used as the last frame or call when a query counts all events. */
        constexpr uint64_t NO_LIMIT = UINT64_MAX;

        /* The point up to which the events of a timeline are counted: the frames up to and including last_frame_index, and the calls up to and
         * including last_call_index.
         */
        struct LevelCut
        {
            uint64_t last_call_index  = NO_LIMIT;
            uint64_t last_frame_index = NO_LIMIT;
        };

        /* A rectangle, in texels, of a run of layers of a level. One event provided its content, and no later event has overwritten it. */
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

        /* What one level of a texture holds at a point of the timeline, as far as the events tell: which event made each part of it. A part that
         * no piece covers was never written by an event (the level got a size but no pixels, or it does not exist yet). Pieces made by rendering
         * (RENDER_WRITE) have no bytes in the timeline. They mark the places where a snapshot has to be read.
         */
        struct LevelContent
        {
            bool                    has_level                  = false; /* an event gave the level a size, at or before the cut */
            bool                    has_snapshot               = false;
            uint32_t                level_depth_or_layer_count = 1;
            Utils::Graphics::TextureFormat level_format        = Utils::Graphics::TextureFormat::UNKNOWN;
            uint32_t                level_height               = 0;
            uint32_t                level_width                = 0;
            std::vector<LevelPiece> piece_vec;
            uint32_t                snapshot_event_index       = 0;     /* the last SNAPSHOT of the level, at or before the cut */
        };

        /* Everything in a .textures file except the bytes of the blobs. */
        struct TextureTimeline
        {
            std::vector<BlobEntry>                           blob_vec;
            uint64_t                                         blob_file_n_bytes      = 0; /* the size of the file of blobs, so that it can be told apart from another file */
            std::vector<TextureEvent>                        event_vec;
            bool                                             includes_render_content = false;
            std::vector<Analysis::APIDumpAnalysis::Resource> resource_vec;
            Analysis::APIDumpAnalysis::WorkloadReference     workload_reference;
        };
    }
}

#endif /* APIDUMPTEXTURES_TYPES_H */
