/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Layout of a .deps file. The file stores, for every resource of a workload, how each frame uses the content that earlier frames left in it. The
 * analysis runs once and the file is then queried as often as needed. Integers are little-endian.
 *
 *   file := FileHeader section*          (an AnalysisFileHeader: magic, file version, analysis version; the sections are those of SectionedFile, in this order, once each)
 *
 *   META  the workload (name, size, and hashes of its start, middle and end), properties of the dump (truncation, pointer size, timer frequency),
 *         and the number of calls per API
 *   RSRC  the resources, as in a .passes file (see PassesSerializer::serialize_resources)
 *   FCAL  the first and the last call of each frame
 *   RDEP  for each resource: its totals, the runs of frames that use it in the same way, the versions of its content (each with the frames that
 *         use it, its inputs, and the rectangles its writer wrote if every write to the resource reported where it landed), and then the written
 *         extent that contains all those rectangles, if there is one
 *
 * The file does not contain workload data. ANALYSIS_VERSION identifies the meaning of the section contents (see DependencyAnalyzer). Increase it
 * whenever the same workload would produce a different file.
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
            constexpr uint32_t ANALYSIS_VERSION = 2;
            constexpr uint32_t FILE_VERSION     = 2;
            constexpr char     MAGIC[8]         = {'A', 'P', 'I', 'D', 'E', 'P', 'S', 'F'};

            constexpr uint32_t TAG_FCAL = Analysis::APIDumpAnalysis::make_section_tag('F', 'C', 'A', 'L');
            constexpr uint32_t TAG_META = Analysis::APIDumpAnalysis::make_section_tag('M', 'E', 'T', 'A');
            constexpr uint32_t TAG_RDEP = Analysis::APIDumpAnalysis::make_section_tag('R', 'D', 'E', 'P');
            constexpr uint32_t TAG_RSRC = Analysis::APIDumpAnalysis::make_section_tag('R', 'S', 'R', 'C');

            typedef Analysis::APIDumpAnalysis::AnalysisFileHeader FileHeader;
        }
    }
}

#endif /* APIDUMPDEPENDENCIES_DEPS_FORMAT_H */
