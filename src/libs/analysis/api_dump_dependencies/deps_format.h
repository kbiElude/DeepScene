/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Layout of a .deps file: per resource, how each frame uses the content earlier frames left in it. Integers are little-endian.
 *
 *   file := FileHeader section*   (an AnalysisFileHeader: magic, file version, analysis version; SectionedFile sections, in this order, once each)
 *
 *   META  the workload (name, size, hashes of start, middle and end), dump properties (truncation, pointer size, timer frequency), calls per API
 *   RSRC  the resources, as in a .passes file (see PassesSerializer::serialize_resources)
 *   FCAL  the first and last call of each frame
 *   RDEP  per resource: totals, runs of frames using it alike, content versions (each with its users, its inputs, and the rectangles its writer
 *         wrote if every write reported where it landed), then the written extent containing all those rectangles, if any
 *
 * No workload data is stored. ANALYSIS_VERSION identifies the meaning of the section contents (see DependencyAnalyzer); increase it whenever
 * the same workload would produce a different file.
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
