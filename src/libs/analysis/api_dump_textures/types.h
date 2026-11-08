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
#include <stdint.h>
#include <vector>

namespace Analysis
{
    namespace APIDumpTextures
    {
        /* What is done to the bytes of a content before they are compressed. Neither loses anything. The values are stored: they do not change. */
        enum class BlobFilter : uint8_t
        {
            NONE            = 0,
            BYTE_PLANES_OF_4 = 1  /* the bytes 0, 4, 8... first, then 1, 5, 9..., and so on: runs of alike bytes for what is made of 32-bit pixels; a rest of less than 4 bytes stays as it is */
        };

        /* What an event does to a texture. The values are stored: they do not change. */
        enum class TextureEventKind : uint8_t
        {
            UPLOAD        = 0, /* bytes of the application are the content of a region of a level (the blob)                     */
            RENDER_WRITE  = 1, /* the content comes of rendering (a copy from a framebuffer or a render target): no bytes unless asked for */
            GENERATE_MIPS = 2, /* the levels below generated_from_level are made of it, by the driver                              */
            SNAPSHOT      = 3  /* a level that rendering changed is read for the first time since: the content it has here (the blob, once a replay has read it back; none until then) */
        };

        /* Where the bytes of a content are. The values are stored: they do not change. */
        enum class BlobLocation : uint8_t
        {
            STORED         = 0, /* in the file of blobs, compressed (what the workload does not have)                                                      */
            WORKLOAD_CHUNK = 1  /* in the workload, in the data chunk that the upload carried: nothing is copied, and the entry has what tells the chunk is the one */
        };

        /* The content which an upload carried, as it was given: its bytes are not changed in any way, wherever they are. The hashes and the size are those of the bytes
         * themselves, so that a reader can tell that what it got from the file of blobs, or from the workload, is the content.
         */
        struct BlobEntry
        {
            uint64_t     content_hash        = 0;
            uint32_t     data_chunk_id       = 0; /* WORKLOAD_CHUNK only: the chunk of the workload, by its number in the dump */
            uint64_t     file_offset_n_bytes = 0; /* STORED only: of its compressed bytes, in the file of blobs */
            BlobFilter   filter              = BlobFilter::NONE; /* STORED only */
            uint64_t     length_hash         = 0; /* of the length and the bytes: a second hash, so that two contents are one only if both hashes and the length are alike */
            BlobLocation location            = BlobLocation::STORED;
            uint64_t     n_raw_bytes         = 0;
            uint64_t     n_stored_bytes      = 0; /* STORED only */
        };

        /* How the bytes of an upload are laid out, as the front end that wrote it says: the values mean something to it alone (as Format::raw_value does). */
        struct SourceLayout
        {
            uint32_t api_format         = 0;
            uint32_t api_type           = 0;
            uint32_t row_pitch_n_bytes  = 0; /* 0: the rows follow each other */
        };

        constexpr uint32_t NO_BLOB = UINT32_MAX;

        struct TextureEvent
        {
            uint32_t                                blob_index           = NO_BLOB;
            uint64_t                                call_index           = 0;
            Analysis::APIDumpAnalysis::ResourceView destination;
            uint64_t                                frame_index          = 0;
            uint32_t                                generated_from_level = 0;     /* GENERATE_MIPS only */
            uint32_t                                height               = 0;
            TextureEventKind                        kind                 = TextureEventKind::UPLOAD;
            SourceLayout                            layout;

            /* The level as it is after the event, so that the size and the format of a level at any time are those of the last event that names it: an upload of the
             * whole level may give it another size or format than it had (a texture that is specified again), and the level is then what the event says.
             */
            Analysis::APIDumpAnalysis::Format       level_format;
            uint32_t                                level_height         = 0;
            uint32_t                                level_width          = 0;

            Analysis::APIDumpAnalysis::ResourceView source;                       /* RENDER_WRITE only; no resource: the window (the swapchain), which is never stored */
            uint32_t                                width                = 0;
            uint32_t                                x                    = 0;
            uint32_t                                y                    = 0;
        };

        /* Everything of a .textures file but the bytes of the blobs. */
        struct TextureTimeline
        {
            std::vector<BlobEntry>                           blob_vec;
            uint64_t                                         blob_file_n_bytes      = 0; /* of the file the blobs are in, so that it is told from another */
            std::vector<TextureEvent>                        event_vec;
            bool                                             includes_render_content = false;
            std::vector<Analysis::APIDumpAnalysis::Resource> resource_vec;
            Analysis::APIDumpAnalysis::WorkloadReference     workload_reference;
        };
    }
}

#endif /* APIDUMPTEXTURES_TYPES_H */
