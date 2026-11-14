/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Finds the draws that may be 2D overlays, by two opinions: the recipe's (the draws it leaves alone, by their keys as a replay asks, and by their signatures as a
 * converter that plays the application as it runs asks, over the cameras the pass has), and the hints of the analysis (the camera of the pass never changes; see
 * QueryCommands::get_static_camera_pass_id_vec; or the draw was drawn with an orthographic projection; see DrawStructure::is_orthographic). The draws on which
 * they disagree are the ones for a person to look at.
 */
#ifndef RECIPEBAKE_OVERLAY_CANDIDATE_FINDER_H
#define RECIPEBAKE_OVERLAY_CANDIDATE_FINDER_H

#include "utils/json/json_writer.h"
#include "analysis/api_dump_analysis/model.h"
#include "analysis/recipe/recipe_applier.h"
#include "types.h"
#include "analysis/recipe/types.h"
#include <map>
#include <vector>

namespace Analysis
{
    namespace RecipeBake
    {
        class OverlayCandidateFinder
        {
        public:
            /* The draws of the passes of the analysis (one for each signature of a pass) that either opinion calls an overlay, in the order of the passes. */
            static std::vector<OverlayCandidate> find(const Analysis::Recipe::Recipe&                  in_recipe,
                                                      const Analysis::APIDumpAnalysis::AnalysisResult& in_analysis);

            static const char* get_agreement_name (const OverlayAgreement& in_agreement);
            static const char* get_live_class_name(const LiveClass&        in_live_class);

            /* Writes the candidates as JSON, with keys as hex strings. */
            static void write_json(const std::vector<OverlayCandidate>& in_candidate_vec,
                                   const Analysis::Recipe::Recipe&      in_recipe,
                                   Utils::Json::JsonWriter*             inout_writer_ptr);

            /* The facts of the draws of each key (signature and size), in all the structures that have it, as if they were one group. */
            static std::map<uint64_t, Analysis::APIDumpAnalysis::DrawFacts> get_facts_by_cover_key(const Analysis::APIDumpAnalysis::AnalysisResult& in_analysis);

            /* The frames of a draw to look at it in, from all the frames it is in: the first, the middle one and the last (one frame, or two, if that is all there are). */
            static std::vector<uint64_t> get_sample_frame_index_vec(const std::vector<uint64_t>& in_frame_index_vec);

        private:
            /* The facts of two groups of draws, as if they were one. */
            static Analysis::APIDumpAnalysis::DrawFacts merge_draw_facts(const Analysis::APIDumpAnalysis::DrawFacts& in_a,
                                                                         const Analysis::APIDumpAnalysis::DrawFacts& in_b);

            /* What the converter does with the draws of the signature, for each of the cameras of the pass; a pass with no camera has one with no numbers in it. */
            static LiveClass get_live_class(const Analysis::Recipe::RecipeApplier&           in_applier,
                                            const Analysis::APIDumpAnalysis::AnalysisResult& in_analysis,
                                            const uint32_t&                                  in_pass_id,
                                            const uint64_t&                                  in_signature_key);
        };
    }
}

#endif /* RECIPEBAKE_OVERLAY_CANDIDATE_FINDER_H */
