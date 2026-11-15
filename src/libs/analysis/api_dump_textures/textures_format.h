/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Layout of a .textures file and of the file of blobs next to it. Integers are little-endian.
 *
 * The .textures file records what happens to the textures of a workload and where the bytes of each content are:
 *
 *   file := FileHeader section*          (an AnalysisFileHeader: magic, file version, analysis version; the sections are those of SectionedFile, in this order, once each)
 *
 *   META  the workload (name, size, and hashes of its start, middle and end), whether render content was requested, and the size of the file of blobs
 *   RSRC  the resources, as in a .passes file (see PassesSerializer::serialize_resources)
 *   BLOB  the contents: where each is in the file of blobs, its filter, its size, and its hashes
 *   EVNT  the events of the textures, in the order of the calls
 *
 * A content is stored in one of two ways. Bytes that an upload carried are already in the workload, so nothing is copied. The table says which
 * data chunk holds them and records their size and hashes, so that a reader can check that the chunk is the right one. Content that rendering
 * produced (a copy from a framebuffer or a render target) is not in the workload. When something first reads a level that rendering changed, a
 * SNAPSHOT event records the content of the level at that call. A replay reads the snapshot back and stores it in the file of blobs, if it was
 * asked to. The analysis alone only records where the snapshots have to be taken.
 *
 * The file of blobs has the name of the .textures file with ".data" appended. It has a header, followed by the contents one after another. Each
 * content is a Zstandard frame with its size and a checksum of its bytes, so that a reader can load any one content alone and detect a damaged
 * one. Nothing is stored lossily: a reader gets back exactly the bytes that were given. The window (the swap chain) has no content in either file.
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
