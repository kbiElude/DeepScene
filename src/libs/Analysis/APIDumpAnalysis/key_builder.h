/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Builds match keys: 64-bit digests (see Hasher, for what a key is against a hash) which say what a thing is, in a way that
 * does not depend on the run. Two things which the application made the same way have the same key, whatever the addresses,
 * the order of the creation calls (which several threads can change) or the time were. A key is used to recognize a pass or a
 * resource again, in another run, so it never includes anything of the kind.
 */
#ifndef APIDUMPANALYSIS_KEY_BUILDER_H
#define APIDUMPANALYSIS_KEY_BUILDER_H

#include "Analysis/APIDumpAnalysis/types.h"
#include <vector>

namespace Analysis
{
    namespace APIDumpAnalysis
    {
        class KeyBuilder
        {
        public:
            /* Of a constant as the passes are told apart by it. */
            static uint64_t compute_constant_signature_key(const ConstantSignature& in_signature);

            /* Of everything that makes a resource what it is when it is created: its kind, format, extents, flags, and the
             * contents if it starts with immutable ones. Not of its name, address or ID.
             */
            static uint64_t compute_description_key(const ResourceDescription& in_description);

            /* Of a draw as it can be recognized at the moment it is issued, by what is known then and nothing else: the structure and
             * the targets are keyed by how their resources were made (see ResourceKeyTable::get_unwritten_key), not by which pass wrote
             * to them. The pass a draw belongs to is not known until the pass ends; this key is what a live converter can match.
             */
            /* The key that tells apart the draws of a signature by how large they are (the number of vertices, or of indices) and how they are
             * put together: one dynamic buffer holds the quad of a flash and the quads of a text, with the same signature, but not the same count.
             */
            static uint64_t compute_draw_cover_key(const uint64_t&          in_signature_key,
                                                   const uint32_t&          in_vertex_count,
                                                   const PrimitiveTopology& in_topology);

            static uint64_t compute_draw_signature_key(const uint64_t&              in_unwritten_structure_key,
                                                       const std::vector<uint64_t>& in_unwritten_color_view_key_vec,
                                                       const uint64_t&              in_unwritten_depth_stencil_view_key);

            /* Of a draw as the passes are told apart by it. */
            static uint64_t compute_draw_structure_key(const DrawStructure& in_structure);

            /* Of a set of keys, which has no order. */
            static uint64_t compute_key_set_key(std::vector<uint64_t> in_key_vec);

            /* Of everything in a pass structure but its own key. The draws and constants must be sorted, as they are in the
             * structure.
             */
            static uint64_t compute_pass_structure_key(const PassStructure& in_structure);
        };
    }
}

#endif /* APIDUMPANALYSIS_KEY_BUILDER_H */
