/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Layout of a .passes file, shared by the writer, the reader and the tests. Integers are little-endian.
 *
 *   file    := FileHeader section*
 *   section := SectionHeader stored_bytes
 *
 * The stored bytes are a zstd frame that decompresses to n_raw_bytes bytes, whose XXH64 is raw_hash. A reader skips sections it
 * does not know. Each section has its own version; a known section of an unknown version is an error. Sections come in this
 * order, once each:
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
 *   DEPT  how deep the draws of each pass structure are (see DepthHistogram)
 *   SURF  how deep the surfaces that the draws of each pass structure draw are (the same, by vertices)
 *   COVR  the keys of the draws that cover the whole of the view (a signature and a size)
 *
 * Workload data (textures, vertices) is not stored; chunk IDs point into the workload.
 *
 * key_scheme_version is the version of the match key algorithm (see KeyBuilder). Bump it whenever a key would change for the
 * same input; keys of files with different versions cannot be compared.
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
            constexpr uint32_t FILE_VERSION       = 1;
            constexpr uint32_t KEY_SCHEME_VERSION = 1;

            /* Version of the draw signature (see KeyBuilder::compute_draw_signature_key), which a recipe made for a converter that runs
             * with the application names draws by. Bump it whenever a signature would change for the same input.
             */
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

            /* The header of a section is the one that all of the files of the analysis have (see SectionedFile). */
            typedef Analysis::APIDumpAnalysis::SectionHeader SectionHeader;

            /* How deep the draws are: for each pass of each kind of frame, a histogram of the weight of its draws by their distance
             * from the camera, which the convergence of a recipe is taken from. */
            constexpr uint32_t TAG_DEPT = make_tag('D', 'E', 'P', 'T');

            /* How deep the surfaces of the draws are: the same as DEPT, from the vertices of the draws and not the origins of their objects. A file
             * without it was made when the analysis did not read vertices. */
            constexpr uint32_t TAG_SURF = make_tag('S', 'U', 'R', 'F');

            /* The keys of the draws that cover the whole of the view (see Model::covering_draw_key_set). A file without it was made
             * before the analysis told them. */
            constexpr uint32_t TAG_COVR = make_tag('C', 'O', 'V', 'R');

            /* The draw structures: what each kind of draw is (its shaders, vertex layout and buffers, textures, state, and the views
             * it renders to), by the key that tells the draws apart. */
            constexpr uint32_t TAG_DRAW = make_tag('D', 'R', 'A', 'W');

            /* One record for each frame: when it began and ended, the calls it spans, how many draws and passes it has, and the
             * number of its frame shape. */
            constexpr uint32_t TAG_FRAM = make_tag('F', 'R', 'A', 'M');

            /* What the analysis is of and how it was made: the workload (its name, its size and the hash of its start), the clock, the
             * numbers of calls made, ignored, not followed and not supported, the resolution hazards found, and the key collisions. */
            constexpr uint32_t TAG_META = make_tag('M', 'E', 'T', 'A');

            /* The pass structures: each kind of pass (where it renders to and how, its kinds of draws and its camera constants),
             * numbered as the frame shapes refer to them. */
            constexpr uint32_t TAG_PASS = make_tag('P', 'A', 'S', 'S');

            /* What the frames did with each resource: the first read, the first and the last write, and in how many frames it was
             * read, written or carried over from the frame before. */
            constexpr uint32_t TAG_RACC = make_tag('R', 'A', 'C', 'C');

            /* The representative frames of each frame shape: which frames, their weight, and the detail of their passes and of the
             * copies between resources. */
            constexpr uint32_t TAG_REPR = make_tag('R', 'E', 'P', 'R');

            /* The resources the application made: when each was created and destroyed, what it is (its description), its match key
             * and the resource it is a part of, if it is. */
            constexpr uint32_t TAG_RSRC = make_tag('R', 'S', 'R', 'C');

            /* The time series of the camera constants: for each constant of each kind of pass, how often it was updated, and a
             * sample of its values with the time of each. */
            constexpr uint32_t TAG_SERI = make_tag('S', 'E', 'R', 'I');

            /* The frame shapes: each kind of frame, as the kinds of passes it is made of, in order, numbered as the frames and the
             * representative frames refer to them. */
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
