/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Bakes a recipe: from its pass and draw lists (Recipe::affected, ::unaffected, ::skipped) and the analysis, it writes the "baked" section
 * (see recipe_loader.h) holding what a live converter can know of a draw when it is issued. The rest is kept; the file is rewritten in this
 * class's formatting. A live converter knows only the signature (see MatchKeyBuilder::compute_draw_signature_key) and the view matrix,
 * so one rule per signature; if passes of different classes share a signature their view matrices must differ, else baking fails.
 * Also bakes the median vertex depth as the convergence distance per affected signature (Recipe::convergence_vec, see DepthHistogram)
 * and per frame shape (Recipe::scene_convergence_vec).
 */
#ifndef RECIPEBAKE_RECIPE_BAKER_H
#define RECIPEBAKE_RECIPE_BAKER_H

#include "utils/json/json_reader.h"
#include "utils/json/json_writer.h"
#include "analysis/api_dump_analysis/model.h"
#include "analysis/api_dump_dependencies/types.h"
#include "analysis/recipe/recipe_applier.h"
#include "types.h"
#include "analysis/recipe/types.h"
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
            /* Returns false, and says why, if the recipe is invalid or cannot be baked. An existing "baked" section is replaced.
             * The dependencies (see APIDumpDependencyExtractor) are optional and must come from the analysed workload; they say which
             * textures each eye copies. Without them it is the resources a frame reads before writing, and writes, which misses a
             * target read in the frame after the one that rendered to it.
             */
            static bool bake(const std::string&                                          in_recipe_text,
                             const Analysis::APIDumpAnalysis::AnalysisResult&            in_analysis,
                             std::string*                                                out_text_ptr,
                             BakeReport*                                                 out_report_ptr,
                             std::string*                                                out_error_ptr,
                             const Analysis::APIDumpDependencies::DependencyFileContent* in_dependencies_ptr = nullptr);

            /* Depth of each scene in shape order. has_depth is false for a scene with no affected pass with anything in front of the
             * camera. The Comfort near and far settings decide which surfaces count.
             */
            static std::vector<SceneMeasure> measure_scenes(const Analysis::Recipe::Recipe&                  in_recipe,
                                                            const Analysis::APIDumpAnalysis::AnalysisResult& in_analysis);

            /* Convergence and maximum separation for the scene, from the measured distance and the Comfort limits. A SceneComfort entry
             * overrides them.
             */
            static SceneLimits resolve_scene_limits(const Analysis::Recipe::Recipe& in_recipe,
                                                    const SceneMeasure&             in_measure);

        private:
            /* The class of a draw and whether it is pushed, as a live converter sees them. */
            typedef std::pair<Analysis::Recipe::DrawClass, bool>               ClassKey;
            typedef std::map<ClassKey, std::set<uint32_t> >                    ClassMap;
            typedef std::vector<Analysis::Recipe::DrawConvergence>             ConvergenceVector;
            typedef std::vector<std::array<float, 16> >                        MatrixVector;
            typedef std::vector<Analysis::Recipe::PerEyeTexture>               PerEyeTextureVector;
            typedef std::vector<Analysis::Recipe::DrawRule>                    RuleVector;
            typedef std::vector<Analysis::Recipe::SceneConvergence>            SceneConvergenceVector;
            typedef std::vector<Analysis::Recipe::SceneSeparation>             SceneSeparationVector;

            /* The rules for a signature drawn by passes of different classes: each unaffected or pushed class is selected by its passes'
             * view matrices. Returns false, and says why, if the matrices do not tell the classes apart.
             */
            static bool compute_conditional_rules(const Analysis::APIDumpAnalysis::AnalysisResult& in_analysis,
                                                  const uint64_t&                                  in_signature_key,
                                                  const ClassMap&                                  in_class_map,
                                                  RuleVector*                                      inout_rule_vec_ptr,
                                                  std::string*                                     out_error_ptr);
            static void compute_convergence      (const Analysis::Recipe::Recipe&                  in_recipe,
                                                  const Analysis::APIDumpAnalysis::AnalysisResult& in_analysis,
                                                  ConvergenceVector*                               out_convergence_vec_ptr,
                                                  BakeReport*                                      out_report_ptr);
            static bool compute_draw_rules       (const Analysis::Recipe::Recipe&                  in_recipe,
                                                  const Analysis::APIDumpAnalysis::AnalysisResult& in_analysis,
                                                  std::vector<uint64_t>*                           out_known_signature_key_vec_ptr,
                                                  RuleVector*                                      out_rule_vec_ptr,
                                                  BakeReport*                                      out_report_ptr,
                                                  std::string*                                     out_error_ptr);

            static void compute_scene_convergence(const Analysis::Recipe::Recipe&                  in_recipe,
                                                  const Analysis::APIDumpAnalysis::AnalysisResult& in_analysis,
                                                  SceneConvergenceVector*                          out_scene_convergence_vec_ptr,
                                                  SceneSeparationVector*                           out_scene_separation_vec_ptr,
                                                  BakeReport*                                      out_report_ptr);

            /* Classifies draws by what the analysis saw: pixel-space draws are layers, draws always after a 3D draw are overlays (not if
             * they sample a copy of the picture), draws at the camera with vertices in front of it are eye space. Keys whose draws
             * disagree are left out.
             */
            static DrawKeyLists get_automatic_layers(const Analysis::APIDumpAnalysis::AnalysisResult& in_analysis);

            /* Keys of the draws covering the whole view, from the passes the recipe leaves alone (a scene mesh can share the key). */
            static std::vector<uint64_t> get_covering_draw_key_vec(const Analysis::Recipe::Recipe&                  in_recipe,
                                                                   const Analysis::APIDumpAnalysis::AnalysisResult& in_analysis);

            /* Textures carried over from the previous frame that each eye copies. Excludes the back buffer, which the player keeps apart. */
            static PerEyeTextureVector get_per_eye_texture_vec(const Analysis::APIDumpAnalysis::AnalysisResult&            in_analysis,
                                                               const Analysis::APIDumpDependencies::DependencyFileContent* in_dependencies_ptr);

            /* The class as it is written in a message: "affected" or "unaffected, pushed". */
            static std::string get_class_key_text(const ClassKey& in_class_key);

            static const char* get_class_name(const Analysis::Recipe::DrawClass& in_class);

            /* A key as it is written in a recipe. */
            static std::string get_key_text(const uint64_t& in_key);

            /* X scale of a pass's projection, 1 / tan(half the angle of view), from the first camera value; 0 if no projection. */
            static float get_projection_scale(const Analysis::APIDumpAnalysis::AnalysisResult& in_analysis,
                                              const uint32_t&                                  in_pass_id);

            /* Keys of flat draws spanning edge to edge (see DrawFacts); a push widens their viewport. */
            static std::vector<uint64_t> get_side_reaching_draw_key_vec(const Analysis::APIDumpAnalysis::AnalysisResult& in_analysis);

            /* The values that the view matrix of a pass took. Returns false, and says why, if the pass cannot provide them. */
            static bool get_view_matrix_vec(const Analysis::APIDumpAnalysis::AnalysisResult& in_analysis,
                                            const uint32_t&                                  in_pass_id,
                                            MatrixVector*                                    out_matrix_vec_ptr,
                                            std::string*                                     out_reason_ptr);

            static void write_baked(const DrawKeyLists&           in_draw_key_lists,
                                    const ConvergenceVector&      in_convergence_vec,
                                    const std::vector<uint64_t>&  in_known_signature_key_vec,
                                    const PerEyeTextureVector&    in_per_eye_texture_vec,
                                    const RuleVector&             in_rule_vec,
                                    const SceneConvergenceVector& in_scene_convergence_vec,
                                    const SceneSeparationVector&  in_scene_separation_vec,
                                    Utils::Json::JsonWriter*      inout_writer_ptr);
            static void write_value(const Utils::Json::JsonValue& in_value,
                                    Utils::Json::JsonWriter*      inout_writer_ptr);
        };
    }
}

#endif /* RECIPEBAKE_RECIPE_BAKER_H */
