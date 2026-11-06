/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Layout of a .deps file: what the frames of a workload do with what the frames before them left in the resources of it, written once so that
 * it can be asked about as often as needed. Integers are little-endian.
 *
 *   file := FileHeader section*          (the sections are those of SectionedFile, in this order, once each)
 *
 *   META  the workload (its name, its size and the hashes of its start, middle and end), how it was read, and what the calls were of
 *   RSRC  the resources, as in a .passes file (see PassesSerializer::serialize_resources)
 *   FCAL  the calls of each frame: the first and the last
 *   RDEP  for each resource: its totals, the runs of frames which use it in the same way, and the versions of its content with the frames which
 *         use each
 *
 * Workload data is not stored. analysis_version is the version of what the sections mean (see DependencyAnalyzer): bump it whenever a file made
 * from the same workload would come out different.
 */
#ifndef APIDUMPDEPENDENCIES_DEPS_FORMAT_H
#define APIDUMPDEPENDENCIES_DEPS_FORMAT_H

#include "analysis/api_dump_analysis/sectioned_file.h"
#include <stdint.h>

namespace Analysis
{
    namespace APIDumpDependencies
    {
        namespace DepsFormat
        {
            constexpr uint32_t ANALYSIS_VERSION = 1;
            constexpr uint32_t FILE_VERSION     = 1;
            constexpr char     MAGIC[8]         = {'A', 'P', 'I', 'D', 'E', 'P', 'S', 'F'};

            constexpr uint32_t TAG_FCAL = Analysis::APIDumpAnalysis::make_section_tag('F', 'C', 'A', 'L');
            constexpr uint32_t TAG_META = Analysis::APIDumpAnalysis::make_section_tag('M', 'E', 'T', 'A');
            constexpr uint32_t TAG_RDEP = Analysis::APIDumpAnalysis::make_section_tag('R', 'D', 'E', 'P');
            constexpr uint32_t TAG_RSRC = Analysis::APIDumpAnalysis::make_section_tag('R', 'S', 'R', 'C');

            struct FileHeader
            {
                char     magic_i8vec8[8];
                uint32_t version;
                uint32_t analysis_version;
            };

            static_assert(sizeof(FileHeader) == 16,
                          "Unexpected padding in the file header");
        }
    }
}

#endif /* APIDUMPDEPENDENCIES_DEPS_FORMAT_H */
