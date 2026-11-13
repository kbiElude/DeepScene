/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Layout of a .textures file and of the file of blobs beside it. Integers are little-endian.
 *
 * The .textures file says what happens to the textures of a workload, and where the bytes of each content are:
 *
 *   file := FileHeader section*          (an AnalysisFileHeader: magic, file version, analysis version; the sections are those of SectionedFile, in this order, once each)
 *
 *   META  the workload (its name, its size and the hashes of its start, middle and end), whether render content was asked for, and the size of the
 *         file of blobs
 *   RSRC  the resources, as in a .passes file (see PassesSerializer::serialize_resources)
 *   BLOB  the contents: where each is in the file of blobs, how it is filtered, its size, and its hashes
 *   EVNT  the events of the textures, in the order of the calls
 *
 * A content is one of two kinds. What an upload carried is in the workload already, and the table says where (the data chunk, with the size and the hashes of
 * the bytes, so that a reader can tell the chunk is the one): nothing is copied. What rendering made (a copy from a framebuffer, a render target) is not in
 * the workload: when something first reads a level that rendering changed, its content at that call is a snapshot (a SNAPSHOT event), which a replay reads
 * back and stores in the file of blobs, if it was asked to (the analysis alone only says where the snapshots are to be taken). The file of blobs (the name of
 * the .textures file with .data after it) has a header, and then the contents one after the other, each a Zstandard frame with its size and a checksum of
 * its bytes, so that any one is read alone and a damaged one is told. Nothing is stored lossily: what a reader gets of a content is the bytes that were
 * given. The window (the swapchain) has no content in either file.
 */
#ifndef APIDUMPTEXTURES_TEXTURES_FORMAT_H
#define APIDUMPTEXTURES_TEXTURES_FORMAT_H

#include "analysis/api_dump_analysis/sectioned_file.h"
#include <stdint.h>

namespace Analysis
{
    namespace APIDumpTextures
    {
        namespace TexturesFormat
        {
            constexpr uint32_t ANALYSIS_VERSION = 1;
            constexpr uint32_t BLOB_FILE_VERSION = 1;
            constexpr char     BLOB_MAGIC[8]    = {'A', 'P', 'I', 'D', 'T', 'X', 'B', 'F'};
            constexpr uint32_t FILE_VERSION     = 1;
            constexpr char     MAGIC[8]         = {'A', 'P', 'I', 'D', 'T', 'E', 'X', 'F'};

            constexpr uint32_t TAG_BLOB = Analysis::APIDumpAnalysis::make_section_tag('B', 'L', 'O', 'B');
            constexpr uint32_t TAG_EVNT = Analysis::APIDumpAnalysis::make_section_tag('E', 'V', 'N', 'T');
            constexpr uint32_t TAG_META = Analysis::APIDumpAnalysis::make_section_tag('M', 'E', 'T', 'A');
            constexpr uint32_t TAG_RSRC = Analysis::APIDumpAnalysis::make_section_tag('R', 'S', 'R', 'C');

            struct BlobFileHeader
            {
                char     magic_i8vec8[8];
                uint32_t version;
                uint32_t reserved;
            };

            typedef Analysis::APIDumpAnalysis::AnalysisFileHeader FileHeader;

            static_assert(sizeof(BlobFileHeader) == 16,
                          "Unexpected padding in the header of the file of blobs");
        }
    }
}

#endif /* APIDUMPTEXTURES_TEXTURES_FORMAT_H */
