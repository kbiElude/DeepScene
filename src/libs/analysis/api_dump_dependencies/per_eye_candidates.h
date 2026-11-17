/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Finds the resources each eye needs its own copy of: resources that frames render to and whose earlier content a later frame uses (see
 * DependencyAnalyzer). Serves both APIDumpDependencyQuery and the recipe that RecipeBake writes.
 *
 * Whether the using frame also writes the resource does not matter: an eye's next frame must see what that eye's frame rendered, not what
 * the other eye rendered in between. Upload-only textures are the same for both eyes and are not candidates.
 *
 * Targets written in turns form a rotation group (resources of one description used in an order that need not repeat). Eyes treat a group
 * as one unit: if one member is a candidate, all are.
 *
 * Swap chain images are excluded: the player keeps the back buffer content apart per eye, by its pixels.
 */
#ifndef APIDUMPDEPENDENCIES_PER_EYE_CANDIDATES_H
#define APIDUMPDEPENDENCIES_PER_EYE_CANDIDATES_H

#include "analysis/api_dump_dependencies/types.h"
#include <stddef.h>
#include <vector>

namespace Analysis
{
    namespace APIDumpDependencies
    {
        constexpr size_t NO_ROTATION_GROUP = static_cast<size_t>(-1);

        struct PerEyeCandidate
        {
            bool   is_added_by_group   = false;              /* added because its rotation group has a candidate, not by its own use        */
            bool   is_certain          = false;              /* the use of earlier content is certain, not only possible                    */
            size_t n_resource          = 0;                  /* resource number minus 1: index into the analysis resources                  */
            size_t rotation_group_index = NO_ROTATION_GROUP; /* index into find_rotation_groups()                                           */
        };

        /* Resources of one description that are written in turns. */
        struct RotationGroup
        {
            bool                has_regular_order = false; /* a fixed order repeats, each member once per cycle (cycle length = group size) */
            std::vector<size_t> n_resource_vec;            /* resource numbers minus 1, in resource order                   */
        };

        class PerEyeCandidates
        {
        public:
            /* Returns the candidates, sorted by resource. */
            static std::vector<PerEyeCandidate> find(const DependencyFileContent& in_analysis);

            /* Returns the rotation groups, sorted by first resource. A group is 2+ resources of one description that frames render to, each
             * written in at least two frames, with writes alternating between members.
             */
            static std::vector<RotationGroup> find_rotation_groups(const DependencyFileContent& in_analysis);

        private:
            /* True if frames render to the resource (render target or depth-stencil) and it is not a swap chain image. */
            static bool is_rendered_to_by_frames(const Analysis::APIDumpAnalysis::Resource& in_resource);
        };
    }
}

#endif /* APIDUMPDEPENDENCIES_PER_EYE_CANDIDATES_H */
