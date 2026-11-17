/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Layout of a .textures file and of the file of blobs next to it. Integers are little-endian.
 *
 *   file := FileHeader section*   (an AnalysisFileHeader: magic, file version, analysis version; SectionedFile sections, in this order, once each)
 *
 *   META  the workload (name, size, hashes of start, middle and end), whether render content was requested, size of the file of blobs
 *   RSRC  the resources, as in a .passes file (see PassesSerializer::serialize_resources)
 *   BLOB  the contents: position in the file of blobs, filter, size, hashes
 *   EVNT  the texture events, in call order
 *
 * Upload bytes are already in the workload and are not copied: the table names the data chunk and records its size and hashes for verification.
 * Rendered content is not in the workload: a SNAPSHOT event marks the first read of a changed level; a replay stores the snapshot in the file of
 * blobs when asked to. The analysis alone only records where snapshots have to be taken.
 *
 * The file of blobs is named like the .textures file plus ".data". It has a header, then the contents one after another, each a Zstandard frame
 * with its size and a checksum, so one content can be loaded alone and damage detected. Storage is lossless. The window (swap chain) has no content.
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
