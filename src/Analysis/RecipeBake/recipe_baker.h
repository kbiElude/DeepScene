/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Bakes a recipe: from what the recipe says of passes and draws (see Recipe::affected, ::unaffected and ::skipped) and the
 * analysis of the application, it works out what a converter which plays the application as it runs can know of each draw when
 * the draw is issued, and writes that into the "baked" section of the recipe (see recipe_loader.h). The rest of the recipe is
 * kept; the file is written again in the formatting of this class.
 *
 * A converter which plays an application as it runs does not know the pass a draw belongs to until the pass has ended. It knows
 * the signature of the draw (see KeyBuilder::compute_draw_signature_key), and the view matrix. Most signatures are drawn by
 * passes of one class only, and a rule per signature says it. Where passes of different classes draw the same signature (an
 * overlay drawn with the shaders of the scene, say), the view matrix of the passes tells them apart, if the matrices of the
 * classes are never the same; if they are, or there is none, baking fails: that application needs the converter to hold the draws
 * of a pass back until it knows which pass it is, which does not exist yet.
 *
 * It also works out the distance at which the eyes are to converge for each signature that is affected (see
 * Recipe::convergence_vec): the median, by vertices, of how deep the draws of the passes that draw it are in the space of the
 * camera (see DepthHistogram). It is a property of the scene, so that nobody has to calibrate it.
 *
 * Passes that share a signature must share a distance, which blends the scenes they are in. A converter that knows what kind of
 * frame (what scene) the frame before was, by its shape key, can do better, so the distance of each frame shape is baked too (see
 * Recipe::scene_convergence_vec): the median, by vertices, of the depth of the affected passes of the frames of that shape.
 */
#ifndef RECIPEBAKE_RECIPE_BAKER_H
#define RECIPEBAKE_RECIPE_BAKER_H

#include "Analysis/APIDumpAnalysis/json_reader.h"
#include "Analysis/APIDumpAnalysis/json_writer.h"
#include "Analysis/APIDumpAnalysis/model.h"
#include "Analysis/RecipeApplier/recipe_applier.h"
#include "Analysis/RecipeBake/types.h"
#include "Analysis/RecipeLoader/types.h"
#include <array>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace Analysis
{
    namespace RecipeBake
    {
        class RecipeBaker
        {
        public:
            /* Returns false, and says why, if the recipe is not valid or cannot be baked with the analysis. A "baked" section that the
             * recipe has already is replaced.
             */
            static bool bake(const std::string&                               in_recipe_text,
                             const Analysis::APIDumpAnalysis::AnalysisResult& in_analysis,
                             std::string*                                     out_text_ptr,
                             BakeReport*                                      out_report_ptr,
                             std::string*                                     out_error_ptr);

        private:
            typedef std::map<Analysis::RecipeLoader::DrawClass, std::set<uint32_t> > ClassMap;
            typedef std::vector<Analysis::RecipeLoader::DrawConvergence>             ConvergenceVector;
            typedef std::vector<Analysis::RecipeLoader::SceneConvergence>            SceneConvergenceVector;
            typedef std::vector<Analysis::RecipeLoader::SceneSeparation>             SceneSeparationVector;
            typedef std::vector<std::array<float, 16> >                              MatrixVector;
            typedef std::vector<Analysis::RecipeLoader::PerEyeTexture>               PerEyeTextureVector;
            typedef std::vector<Analysis::RecipeLoader::DrawRule>                    RuleVector;

            /* The rules of a signature which passes of different classes draw: each class that is not affected is told by the view
             * matrices of its passes. Returns false, and says why, if the matrices do not tell the classes apart.
             */
            static bool compute_conditional_rules(const Analysis::APIDumpAnalysis::AnalysisResult& in_analysis,
                                                  const uint64_t&                                  in_signature_key,
                                                  const ClassMap&                                  in_class_map,
                                                  RuleVector*                                      inout_rule_vec_ptr,
                                                  std::string*                                     out_error_ptr);
            static void compute_convergence      (const Analysis::RecipeLoader::Recipe&            in_recipe,
                                                  const Analysis::APIDumpAnalysis::AnalysisResult& in_analysis,
                                                  ConvergenceVector*                               out_convergence_vec_ptr,
                                                  BakeReport*                                      out_report_ptr);
            static bool compute_draw_rules       (const Analysis::RecipeLoader::Recipe&            in_recipe,
                                                  const Analysis::APIDumpAnalysis::AnalysisResult& in_analysis,
                                                  std::vector<uint64_t>*                           out_known_signature_key_vec_ptr,
                                                  RuleVector*                                      out_rule_vec_ptr,
                                                  BakeReport*                                      out_report_ptr,
                                                  std::string*                                     out_error_ptr);

            static void compute_scene_convergence(const Analysis::RecipeLoader::Recipe&            in_recipe,
                                                  const Analysis::APIDumpAnalysis::AnalysisResult& in_analysis,
                                                  SceneConvergenceVector*                          out_scene_convergence_vec_ptr,
                                                  SceneSeparationVector*                           out_scene_separation_vec_ptr,
                                                  BakeReport*                                      out_report_ptr);

            /* The keys of the draws that cover the whole of the view, in the passes that the recipe leaves alone: only those are moved by the
             * overlay, and the mesh of a scene can have the same key.
             */
            static std::vector<uint64_t> get_covering_draw_key_vec(const Analysis::RecipeLoader::Recipe&         in_recipe,
                                                                   const Analysis::APIDumpAnalysis::AnalysisResult& in_analysis);

            /* The textures which frames carry over from the frame before, which each eye has a copy of (the back buffer is not one: the player keeps
             * its pictures apart for the eyes). */
            static PerEyeTextureVector get_per_eye_texture_vec(const Analysis::APIDumpAnalysis::AnalysisResult& in_analysis);

            static const char* get_class_name(const Analysis::RecipeLoader::DrawClass& in_class);

            static Analysis::RecipeLoader::DrawClass get_draw_class(const Analysis::RecipeApplier::RecipeApplier& in_applier,
                                                                    const uint64_t&                               in_pass_key,
                                                                    const uint64_t&                               in_structure_key);

            /* As the keys are written in a recipe. */
            static std::string get_key_text(const uint64_t& in_key);

            /* The horizontal scale of the projection of a pass (which is 1 over the tangent of half the angle of view), from the first value that
             * its camera had; 0 if the pass has no projection.
             */
            static float get_projection_scale(const Analysis::APIDumpAnalysis::AnalysisResult& in_analysis,
                                              const uint32_t&                                  in_pass_id);

            /* The values the view matrix of a pass took. Returns false, and says why, if the pass cannot say. */
            static bool get_view_matrix_vec(const Analysis::APIDumpAnalysis::AnalysisResult& in_analysis,
                                            const uint32_t&                                  in_pass_id,
                                            MatrixVector*                                    out_matrix_vec_ptr,
                                            std::string*                                     out_reason_ptr);

            static void write_baked(const std::vector<uint64_t>&                in_covering_draw_key_vec,
                                    const ConvergenceVector&                    in_convergence_vec,
                                    const std::vector<uint64_t>&                in_known_signature_key_vec,
                                    const PerEyeTextureVector&                  in_per_eye_texture_vec,
                                    const RuleVector&                           in_rule_vec,
                                    const SceneConvergenceVector&               in_scene_convergence_vec,
                                    const SceneSeparationVector&                in_scene_separation_vec,
                                    Analysis::APIDumpAnalysis::JsonWriter*      inout_writer_ptr);
            static void write_value(const Analysis::APIDumpAnalysis::JsonValue& in_value,
                                    Analysis::APIDumpAnalysis::JsonWriter*      inout_writer_ptr);
        };
    }
}

#endif /* RECIPEBAKE_RECIPE_BAKER_H */
