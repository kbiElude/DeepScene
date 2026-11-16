/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Compares what the replay and the converter that plays the application as it runs decide for each draw of the analysis, without playing
 * anything. Both ask the recipe (RecipeApplier::decide_draw), but with different keys: the replay knows the pass of a draw once the frame is
 * over, and asks by the key of the pass and of the draw; the converter has only the signature of the draw and the camera of the pass, and asks
 * by those. A recipe is baked so that the two agree. Where they do not, one of the two plays the draw wrongly (as an unaffected draw, say,
 * where the other plays it for both eyes), and the first frame to show it is a long way into a capture.
 *
 * A draw is compared for each size it was seen with (the cover key: the recipe can hold or push a draw by it), and the converter's decision
 * for each camera that its pass was seen with.
 */
#ifndef RECIPEBAKE_PARITY_AUDITOR_H
#define RECIPEBAKE_PARITY_AUDITOR_H

#include "analysis/api_dump_analysis/model.h"
#include "analysis/recipe/recipe_applier.h"
#include "types.h"
#include "analysis/recipe/types.h"
#include <stdio.h>
#include <vector>

namespace Analysis
{
    namespace RecipeBake
    {
        class ParityAuditor
        {
        public:
            /* The draws in the order of the passes of the analysis, and of the draws of each pass; the sizes of a draw are in the order of their keys. */
            static ParityReport audit(const Analysis::Recipe::Recipe&                  in_recipe,
                                      const Analysis::APIDumpAnalysis::AnalysisResult& in_analysis);

            /* Writes the report for a person: how much was compared, and each draw on which the two disagree. */
            static void print_report(FILE*                in_file_handle,
                                     const ParityReport&  in_report);

        private:
            static const char* get_class_name(const Analysis::Recipe::DrawClass& in_draw_class);

            /* The sizes (cover keys) that the analysis saw the draws of the structure with; 0 once if it has no facts of them (an analysis file that has no draw facts). */
            static std::vector<uint64_t> get_cover_key_vec(const Analysis::APIDumpAnalysis::AnalysisResult& in_analysis,
                                                           const uint64_t&                                  in_structure_key);
        };
    }
}

#endif /* RECIPEBAKE_PARITY_AUDITOR_H */
