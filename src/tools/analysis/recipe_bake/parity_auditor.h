/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Compares the replay's and the live converter's decisions for each analysed draw, without playing. Both call RecipeApplier::decide_draw,
 * the replay by pass and draw key, the converter by draw signature and pass camera; a baked recipe makes them agree, and a disagreement
 * means one of them plays the draw wrongly. Compared per size seen (the cover key) and, for the converter, per camera of its pass.
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
            /* Draws in pass order, then draw order within a pass; the sizes of a draw in key order. */
            static ParityReport audit(const Analysis::Recipe::Recipe&                  in_recipe,
                                      const Analysis::APIDumpAnalysis::AnalysisResult& in_analysis);

            /* Writes the report for a person: how much was compared, and each draw on which the two disagree. */
            static void print_report(FILE*                in_file_handle,
                                     const ParityReport&  in_report);

        private:
            static const char* get_class_name(const Analysis::Recipe::DrawClass& in_draw_class);

            /* The sizes (cover keys) the structure's draws were seen with; 0 once if the analysis has no draw facts. */
            static std::vector<uint64_t> get_cover_key_vec(const Analysis::APIDumpAnalysis::AnalysisResult& in_analysis,
                                                           const uint64_t&                                  in_structure_key);
        };
    }
}

#endif /* RECIPEBAKE_PARITY_AUDITOR_H */
