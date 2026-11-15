/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Finds the resources that each eye needs its own copy of: resources that frames render to and whose content from earlier frames is used by a
 * later frame, according to the dependencies of the workload (see DependencyAnalyzer). One rule serves both the answer of
 * APIDumpDependencyQuery and the recipe that RecipeBake writes.
 *
 * It does not matter whether the frame that uses the earlier content also writes the resource. Take two targets that frames render to in turns,
 * where each frame reads the target that the previous frame rendered to. Every frame uses earlier content, but no frame both reads and writes the
 * same target. What matters is that the content an eye's frame renders is what that eye's next frame uses, and not what the other eye's frame
 * rendered in between. A texture that is only uploaded is the same for both eyes and is not a candidate.
 *
 * Targets that are written in turns form a rotation group: any number of resources of one description that the frames use in an order that need
 * not repeat (the images of a swap chain that the application takes in the order the API gives them, or the buffers of a hand-made chain). The
 * eyes treat a group as one unit: if one member is a candidate, all members are, so that an eye never finds the content of the other eye's frame
 * in the target that its own turn comes to.
 *
 * The image of a swap chain is excluded: the player keeps the content that a frame carries over in the back buffer apart for each eye, by its
 * pixels. (An API whose swap chain has several images that the application uses in turns needs this to be reconsidered. See the notes on the
 * front end of such an API.)
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
            size_t n_resource          = 0;                  /* the resource number minus 1, as an index into the resources of the analysis */
            size_t rotation_group_index = NO_ROTATION_GROUP; /* an index into the groups of find_rotation_groups()                          */
        };

        /* Resources of one description that are written in turns. */
        struct RotationGroup
        {
            bool                has_regular_order = false; /* the same order again and again, each member once per cycle (a cycle is as long as the group) */
            std::vector<size_t> n_resource_vec;            /* the resource numbers minus 1, in resource order                                              */
        };

        class PerEyeCandidates
        {
        public:
            /* Returns the candidates, sorted by resource. */
            static std::vector<PerEyeCandidate> find(const DependencyAnalysis& in_analysis);

            /* Returns the rotation groups, sorted by the first resource of each. A group is two or more resources of one description that frames
             * render to. Each is written in at least two frames, and no member is written twice in a row (the writes alternate between members).
             */
            static std::vector<RotationGroup> find_rotation_groups(const DependencyAnalysis& in_analysis);

        private:
            /* True if frames render to the resource (as a render target or a depth-stencil buffer) and it is not the image of a swap chain. */
            static bool is_rendered_to_by_frames(const Analysis::APIDumpAnalysis::Resource& in_resource);
        };
    }
}

#endif /* APIDUMPDEPENDENCIES_PER_EYE_CANDIDATES_H */
