/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Layout of a .passes file, shared by the writer, the reader and the tests. Integers are little-endian.
 *
 *   file    := FileHeader section*
 *   section := SectionHeader stored_bytes
 *
 * The stored bytes are a zstd frame decompressing to n_raw_bytes bytes with XXH64 raw_hash. Readers skip unknown sections; a known section
 * with an unknown version is an error. Each section has its own version. Sections appear once each, in this order:
 *
 *   META  workload reference and how the workload was read
 *   RSRC  resources
 *   DRAW  draw structures
 *   PASS  pass structures
 *   SHPE  frame shapes
 *   FRAM  one record per frame
 *   RACC  what the frames read and wrote, per resource (see ResourceAccess)
 *   REPR  detail of the representative frames of each shape
 *   SERI  time series of the camera constants
 *   DEPT  draw depth per pass structure (see DepthHistogram)
 *   SURF  surface depth per pass structure (same, by vertices)
 *   PROJ  picture width per pass of each frame kind (see ProjectionScale)
 *   COVR  keys of the draws that cover the whole view (a signature and a size)
 *   FACT  what the draws of each structure did: blending, sampling a picture copy, vertex positions (see DrawFacts)
 *
 * Workload data (textures, vertices) is not stored; chunk IDs point into the workload.
 * key_scheme_version versions the match key algorithm (see MatchKeyBuilder); bump it when a key would change for the same input.
 * Keys of files with different versions cannot be compared.
 */
#ifndef APIDUMPANALYSIS_PASSES_FORMAT_H
#define APIDUMPANALYSIS_PASSES_FORMAT_H

#include "analysis/api_dump_analysis/sectioned_file.h"
#include <stdint.h>

namespace Analysis
{
    namespace APIDumpAnalysis
    {
        namespace PassesFormat
        {
            constexpr uint32_t FILE_VERSION       = 4;
            constexpr uint32_t KEY_SCHEME_VERSION = 1;

            /* Version of the draw signature (see MatchKeyBuilder::compute_draw_signature_key); recipes identify draws by it. Increase it when a signature would change. */
            constexpr uint32_t SIGNATURE_SCHEME_VERSION = 1;

            constexpr char     MAGIC[8]           = {'A', 'P', 'I', 'P', 'A', 'S', 'S', 'F'};

            /* The tag of a section: four characters. */
            constexpr uint32_t make_tag(const char& in_a,
                                        const char& in_b,
                                        const char& in_c,
                                        const char& in_d)
            {
                return make_section_tag(in_a,
                                        in_b,
                                        in_c,
                                        in_d);
            }

            /* The header of a section is the same for all files of the analysis (see SectionedFile). */
            typedef Analysis::APIDumpAnalysis::SectionHeader SectionHeader;

            /* Draw depth: per pass of each frame kind, a histogram of draw weight by camera distance. Recipe convergence is computed from it. */
            constexpr uint32_t TAG_DEPT = make_tag('D', 'E', 'P', 'T');

            /* Surface depth: like DEPT, but from the draw vertices, not the object origins. */
            constexpr uint32_t TAG_SURF = make_tag('S', 'U', 'R', 'F');

            /* Picture width: per pass of each frame kind, the vertex-weighted mean x scale of its draws' perspective projection (see ProjectionScale). */
            constexpr uint32_t TAG_PROJ = make_tag('P', 'R', 'O', 'J');

            /* The keys of the draws that cover the whole of the view (see Model::covering_draw_key_map). */
            constexpr uint32_t TAG_COVR = make_tag('C', 'O', 'V', 'R');

            /* What the draws of each structure did (see DrawFacts). */
            constexpr uint32_t TAG_FACT = make_tag('F', 'A', 'C', 'T');

            /* Draw structures: each kind of draw (shaders, vertex layout, buffers, textures, state, render views), by its distinguishing key. */
            constexpr uint32_t TAG_DRAW = make_tag('D', 'R', 'A', 'W');

            /* One record per frame: begin and end time, call span, draw and pass counts, and its frame shape number. */
            constexpr uint32_t TAG_FRAM = make_tag('F', 'R', 'A', 'M');

            /* Workload (name, size, start hash), clock, counts of calls made, ignored, not followed and unsupported, resolution hazards, key collisions. */
            constexpr uint32_t TAG_META = make_tag('M', 'E', 'T', 'A');

            /* Pass structures: each kind of pass (render target, draw kinds, camera constants), numbered as frame shapes refer to them. */
            constexpr uint32_t TAG_PASS = make_tag('P', 'A', 'S', 'S');

            /* Per resource: first read, first and last write, and the frame counts of reads, writes and carry-overs from the previous frame. */
            constexpr uint32_t TAG_RACC = make_tag('R', 'A', 'C', 'C');

            /* Representative frames per shape: which frames, their weight, and the detail of their passes and resource copies. */
            constexpr uint32_t TAG_REPR = make_tag('R', 'E', 'P', 'R');

            /* Resources: creation and destruction time, description, match key, and the parent resource if any. */
            constexpr uint32_t TAG_RSRC = make_tag('R', 'S', 'R', 'C');

            /* Camera constant series: per constant of each pass kind, the update count and a sample of values with times. */
            constexpr uint32_t TAG_SERI = make_tag('S', 'E', 'R', 'I');

            /* Frame shapes: each frame kind as an ordered list of pass kinds, numbered as frames and representative frames refer to them. */
            constexpr uint32_t TAG_SHPE = make_tag('S', 'H', 'P', 'E');

            struct FileHeader
            {
                char     magic_i8vec8[8];
                uint32_t version;
                uint32_t key_scheme_version;
            };

            static_assert(sizeof(FileHeader) == 16,
                          "Unexpected padding in the file header");
        }
    }
}

#endif /* APIDUMPANALYSIS_PASSES_FORMAT_H */
