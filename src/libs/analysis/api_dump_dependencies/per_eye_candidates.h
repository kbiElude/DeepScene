/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Which resources each eye is to have a copy of: those which are rendered to, and which some frame uses what an earlier frame left in, as far as
 * the dependencies of the workload say (see DependencyAnalyzer). One rule, for the question that APIDumpDependencyQuery answers and for the recipe
 * that RecipeBake writes.
 *
 * It does not matter whether the frame which uses the content writes the resource too: two targets which frames render to by turns, each
 * reading the one that the frame before rendered to, are used by every frame without either being written and read by the same one. What matters is
 * that what an eye's frame renders is what that eye's next frame uses, and not what the other eye's frame rendered after it. A texture which is
 * only uploaded is the same for both eyes, and is not one.
 *
 * Targets which are written by turns are a rotation group: any number of resources of one description, which the frames take in an order that need
 * not repeat (the images of a swap chain which the application takes as the API gives them, or the buffers of a hand made chain). They are one
 * thing for the eyes: if one of them is a candidate, all of them are, so that an eye never finds the content of another eye's frame in the one which
 * its own turn comes to.
 *
 * The image of a swap chain is left out: the player keeps what a frame carries over in the back buffer apart for the eyes, by its pixels. (An API
 * whose swap chain has several images, which the application takes by turns, needs that to be thought of again; see the notes on the front end of
 * such an API.)
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
            bool   is_added_by_group   = false;              /* it is not one by what the frames do to it, but another resource of its rotation group is */
            bool   is_certain          = false;              /* what a frame uses of what the frames before it left is certain (CarryKind::CERTAIN), not only possible */
            size_t n_resource          = 0;                  /* the number of the resource less 1, in the resources of the analysis                     */
            size_t rotation_group_index = NO_ROTATION_GROUP; /* in the groups that find_rotation_groups() gives                                           */
        };

        /* Resources of one description which are written by turns. */
        struct RotationGroup
        {
            bool                has_regular_order = false; /* the same order again and again, with each resource once in a turn: a cycle of the size of the group */
            std::vector<size_t> n_resource_vec;            /* the numbers of the resources less 1, in the order of the resources                                 */
        };

        class PerEyeCandidates
        {
        public:
            /* In the order of the resources. */
            static std::vector<PerEyeCandidate> find(const DependencyAnalysis& in_analysis);

            /* In the order of the first resource of each. Two or more resources of one description which are rendered to, are each written in two or
             * more frames, and which no frame writes twice in a row (a turn of one is followed by a turn of another).
             */
            static std::vector<RotationGroup> find_rotation_groups(const DependencyAnalysis& in_analysis);
        };
    }
}

#endif /* APIDUMPDEPENDENCIES_PER_EYE_CANDIDATES_H */
