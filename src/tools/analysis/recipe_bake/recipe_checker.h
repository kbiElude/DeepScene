/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Checks a baked recipe against the analysis of a workload, without replaying it. It reports the kinds of frames (shapes) and the draw
 * signatures that the workload has and the recipe does not know. A frame of an unknown shape is played as the recipe's
 * unknown_frame_policy says (by default as it is, for both eyes). A draw of an unknown signature counts as affected. Both are left to
 * chance when the recipe was made from another capture, or from a shorter one. A recipe that lists no shapes or signatures (it is not
 * baked) knows everything.
 *
 * It also checks what the recipe leaves alone (by pass key, draw key or rule), in the same way as OverlayCandidateFinder. A pass whose
 * vertices are at many depths is a 3D scene, which the push of an overlay would turn into a flat picture. A pass that a replay leaves
 * alone, but that the converter playing the application as it runs does not (it cannot tell passes apart), is played in two ways.
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

            /* Writes the report for a person: the kinds of frames and the draw signatures that are not known, with the first frame of each, and what
             * is wrong with the passes that the recipe leaves alone.
             */
            static void print_report(FILE*                           in_file_handle,
                                     const CoverageReport&           in_report,
                                     const Analysis::Recipe::Recipe& in_recipe);

        private:
            /* Whether the draws of a key are of mixed kinds: some in pixel space and some not, some over a 3D scene and some not, or some in eye
             * space and some not.
             */
            static bool is_mixed(const Analysis::APIDumpAnalysis::DrawFacts& in_facts);

            static UnaffectedWarning make_warning(const OverlayCandidate&      in_candidate,
                                                  const UnaffectedWarningKind& in_kind);
        };
    }
}

#endif /* RECIPEBAKE_RECIPE_CHECKER_H */
