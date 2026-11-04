/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Checks a baked recipe against the analysis of a workload, without replaying it: which kinds of frames (shapes) and which draw signatures the workload
 * has that the recipe does not know. A frame of a shape that is not known is played as the recipe's unknown_frame_policy says (as it is, for both eyes, by
 * default), and a draw of a signature that is not known is an affected one: both are what a recipe made of another capture, or of a shorter one, leaves to
 * chance. A recipe that lists none (it is not baked) knows everything.
 */
#ifndef RECIPEBAKE_RECIPE_CHECKER_H
#define RECIPEBAKE_RECIPE_CHECKER_H

#include "Analysis/APIDumpAnalysis/model.h"
#include "Analysis/RecipeBake/types.h"
#include "Analysis/RecipeLoader/types.h"
#include <stdio.h>

namespace Analysis
{
    namespace RecipeBake
    {
        class RecipeChecker
        {
        public:
            /* The shapes are in the order of the first frame that has them; the signatures in the order of their keys. */
            static CoverageReport check(const Analysis::RecipeLoader::Recipe&            in_recipe,
                                        const Analysis::APIDumpAnalysis::AnalysisResult& in_analysis);

            /* Says what the report has, for a person: the kinds of frames and the draw signatures that are not known, with where the first frame of each is. */
            static void print_report(FILE*                                 in_file_handle,
                                     const CoverageReport&                 in_report,
                                     const Analysis::RecipeLoader::Recipe& in_recipe);
        };
    }
}

#endif /* RECIPEBAKE_RECIPE_CHECKER_H */
