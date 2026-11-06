/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Finds the draws that may be 2D overlays, by two opinions: the recipe's (the draws it leaves alone, by their keys as a replay asks, and by their signatures as a
 * converter that plays the application as it runs asks, over the cameras the pass has), and the hint of the analysis (the camera of the pass never changes; see
 * QueryCommands::get_static_camera_pass_id_vec). The draws on which they disagree are the ones for a person to look at.
 */
#ifndef RECIPEBAKE_OVERLAY_CANDIDATE_FINDER_H
#define RECIPEBAKE_OVERLAY_CANDIDATE_FINDER_H

#include "Analysis/APIDumpAnalysis/json_writer.h"
#include "Analysis/APIDumpAnalysis/model.h"
#include "Analysis/RecipeApplier/recipe_applier.h"
#include "types.h"
#include "Analysis/RecipeLoader/types.h"
#include <vector>

namespace Analysis
{
    namespace RecipeBake
    {
        class OverlayCandidateFinder
        {
        public:
            /* The draws of the passes of the analysis (one for each signature of a pass) that either opinion calls an overlay, in the order of the passes. */
            static std::vector<OverlayCandidate> find(const Analysis::RecipeLoader::Recipe&            in_recipe,
                                                      const Analysis::APIDumpAnalysis::AnalysisResult& in_analysis);

            static const char* get_agreement_name(const OverlayAgreement& in_agreement);
            static const char* get_live_class_name(const LiveClass&       in_live_class);

            /* Writes the candidates as JSON, with keys as hex strings. */
            static void write_json(const std::vector<OverlayCandidate>&       in_candidate_vec,
                                   const Analysis::RecipeLoader::Recipe&      in_recipe,
                                   Analysis::APIDumpAnalysis::JsonWriter*     inout_writer_ptr);

        private:
            /* What the converter does with the draws of the signature, for each of the cameras of the pass; a pass with no camera has one with no numbers in it. */
            static LiveClass get_live_class(const Analysis::RecipeApplier::RecipeApplier&  in_applier,
                                            const Analysis::APIDumpAnalysis::AnalysisResult& in_analysis,
                                            const uint32_t&                                in_pass_id,
                                            const uint64_t&                                in_signature_key);
        };
    }
}

#endif /* RECIPEBAKE_OVERLAY_CANDIDATE_FINDER_H */
