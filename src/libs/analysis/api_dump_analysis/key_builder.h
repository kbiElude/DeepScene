/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Builds match keys. A key is a 64-bit digest (see Hasher for how a key differs from a hash) that says what a thing is and does not depend
 * on the run. Two things that the application created the same way have the same key, regardless of addresses, of the order of the creation
 * calls (several threads can change that order) and of timing. A key is used to recognize a pass or a resource in another run, so it never
 * includes any of those.
 */
#ifndef APIDUMPANALYSIS_KEY_BUILDER_H
#define APIDUMPANALYSIS_KEY_BUILDER_H

#include "analysis/api_dump_analysis/types.h"
#include <vector>

namespace Analysis
{
    namespace APIDumpAnalysis
    {
        class KeyBuilder
        {
        public:
            /* The key of a constant, as used to tell passes apart. */
            static uint64_t compute_constant_signature_key(const ConstantSignature& in_signature);

            /* The key of everything that makes a resource what it is at creation: its kind, format, extents, flags, and its contents if it starts
             * with immutable contents. The name, address and ID are not part of it.
             */
            static uint64_t compute_description_key(const ResourceDescription& in_description);

            /* The key that tells apart the draws that share a signature, by their size (the number of vertices or indices) and topology. One
             * dynamic buffer can hold the quad of a flash and the quads of a text with the same signature, but not with the same count.
             */
            static uint64_t compute_draw_cover_key(const uint64_t&          in_signature_key,
                                                   const uint32_t&          in_vertex_count,
                                                   const PrimitiveTopology& in_topology);

            /* The key of a draw as it can be recognized at the moment it is issued, from what is known then. The structure and the targets are
             * keyed by how their resources were created (see ResourceKeyTable::get_unwritten_key), not by which pass wrote to them. The pass of a
             * draw is not known until the pass ends, so this is the key that a live converter can match.
             */
            static uint64_t compute_draw_signature_key(const uint64_t&              in_unwritten_structure_key,
                                                       const std::vector<uint64_t>& in_unwritten_color_view_key_vec,
                                                       const uint64_t&              in_unwritten_depth_stencil_view_key);

            /* The key of a draw, as used to tell passes apart. */
            static uint64_t compute_draw_structure_key(const DrawStructure& in_structure);

            /* The key of a set of keys. The order of the keys does not matter. */
            static uint64_t compute_key_set_key(std::vector<uint64_t> in_key_vec);

            /* The key of everything in a pass structure except its own key. The draws and constants must be sorted, as they are in the
             * structure.
             */
            static uint64_t compute_pass_structure_key(const PassStructure& in_structure);
        };
    }
}

#endif /* APIDUMPANALYSIS_KEY_BUILDER_H */
