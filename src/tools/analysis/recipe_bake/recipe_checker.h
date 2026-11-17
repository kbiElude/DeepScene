/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Checks a baked recipe against a workload's analysis, without replaying: reports frame shapes and draw signatures the recipe does not know
 * (unknown shapes follow unknown_frame_policy, unknown signatures count as affected; an unbaked recipe knows everything). Also checks what
 * the recipe leaves alone, as OverlayCandidateFinder does: a pass at many depths is a 3D scene a push would flatten, and a pass the replay
 * leaves alone but a live converter cannot tell apart is played two ways.
 */
#ifndef RECIPEBAKE_RECIPE_CHECKER_H
#define RECIPEBAKE_RECIPE_CHECKER_H

#include "analysis/api_dump_analysis/model.h"
#include "types.h"
#include "analysis/recipe/types.h"
#include <stdio.h>

namespace Analysis
{
    namespace RecipeBake
    {
        class RecipeChecker
        {
        public:
            /* The shapes are in the order of the first frame that has them; the signatures are in the order of their keys. */
            static CoverageReport check(const Analysis::Recipe::Recipe&                  in_recipe,
                                        const Analysis::APIDumpAnalysis::AnalysisResult& in_analysis);

            /* Writes the report: unknown shapes and signatures with the first frame of each, and problems with passes left alone. */
            static void print_report(FILE*                           in_file_handle,
                                     const CoverageReport&           in_report,
                                     const Analysis::Recipe::Recipe& in_recipe);

        private:
            /* Whether the draws of a key are of mixed kinds (pixel space, over a 3D scene, or eye space: some yes, some no). */
            static bool is_mixed(const Analysis::APIDumpAnalysis::DrawFacts& in_facts);

            static UnaffectedWarning make_warning(const OverlayCandidate&      in_candidate,
                                                  const UnaffectedWarningKind& in_kind);
        };
    }
}

#endif /* RECIPEBAKE_RECIPE_CHECKER_H */
