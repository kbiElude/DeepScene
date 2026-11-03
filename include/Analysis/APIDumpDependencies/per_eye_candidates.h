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
 * The image of a swap chain is left out: the player keeps what a frame carries over in the back buffer apart for the eyes, by its pixels. (An API
 * whose swap chain has several images, which the application takes by turns, needs that to be thought of again; see the notes on the front end of
 * such an API.)
 */
#ifndef APIDUMPDEPENDENCIES_PER_EYE_CANDIDATES_H
#define APIDUMPDEPENDENCIES_PER_EYE_CANDIDATES_H

#include "Analysis/APIDumpDependencies/types.h"
#include <stddef.h>
#include <vector>

namespace Analysis
{
    namespace APIDumpDependencies
    {
        struct PerEyeCandidate
        {
            bool   is_certain  = false; /* what a frame uses of what the frames before it left is certain (CarryKind::CERTAIN), not only possible */
            size_t n_resource  = 0;     /* the number of the resource less 1, in the resources of the analysis                                    */
        };

        class PerEyeCandidates
        {
        public:
            /* In the order of the resources. */
            static std::vector<PerEyeCandidate> find(const DependencyAnalysis& in_analysis);
        };
    }
}

#endif /* APIDUMPDEPENDENCIES_PER_EYE_CANDIDATES_H */
