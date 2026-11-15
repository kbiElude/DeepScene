/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Bakes a recipe. From what the recipe says about passes and draws (see Recipe::affected, ::unaffected and ::skipped) and from the
 * analysis of the application, it works out what a converter that plays the application as it runs can know about each draw when the draw
 * is issued. It writes the result into the "baked" section of the recipe (see recipe_loader.h). The rest of the recipe is kept; the file
 * is written again in the formatting of this class.
 *
 * Such a converter does not know which pass a draw belongs to until the pass has ended. It knows only the signature of the draw (see
 * MatchKeyBuilder::compute_draw_signature_key) and the view matrix. Most signatures are drawn by passes of one class only, so one rule per
 * signature is enough. Some signatures are drawn by passes of different classes (an overlay drawn with the shaders of the scene, say).
 * The view matrix of the passes then tells the classes apart, provided the classes never share a matrix. If they do, or if a pass has no
 * matrix, baking fails: that application needs a converter that holds the draws of a pass back until it knows which pass it is, and this
 * converter does not do that.
 *
 * It also works out the distance at which the eyes converge for each affected signature (see Recipe::convergence_vec): the median over
 * all vertices of how deep the draws of the passes that draw it are in the space of the camera (see DepthHistogram). It is a property of
 * the scene, so nobody has to calibrate it.
 *
 * Passes that share a signature must share a distance, which blends the scenes they are in. A converter that knows from the shape key what
 * kind of frame (what scene) the previous frame was can do better, so the distance of each frame shape is baked too (see
 * Recipe::scene_convergence_vec): the median over all vertices of the depth of the affected passes in the frames of that shape.
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
            /* Returns false, and says why, if the recipe is not valid or cannot be baked with the analysis. An existing "baked" section of the
             * recipe is replaced.
             *
             * The dependencies of the workload (see APIDumpDependencyExtractor) are optional. They say which textures each eye has its own copy of.
             * Without them, those are the resources that a frame reads before it writes them, and writes. That rule misses a target that is read in
             * the frame after the one that rendered to it (two targets that an application uses in turns). The dependencies must come from the
             * workload that the analysis is of.
             */
            static bool bake(const std::string&                                          in_recipe_text,
                             const Analysis::APIDumpAnalysis::AnalysisResult&            in_analysis,
                             std::string*                                                out_text_ptr,
                             BakeReport*                                                 out_report_ptr,
                             std::string*                                                out_error_ptr,
                             const Analysis::APIDumpDependencies::DependencyFileContent* in_dependencies_ptr = nullptr);

            /* How deep each scene (kind of frame) of the analysis is, in the order of the shapes. has_depth is false for a scene that has no affected
             * pass with something in front of the camera. The near and far settings of the recipe (see Comfort) decide which surfaces count.
             */
            static std::vector<SceneMeasure> measure_scenes(const Analysis::Recipe::Recipe&                  in_recipe,
                                                            const Analysis::APIDumpAnalysis::AnalysisResult& in_analysis);

            /* Where the eyes converge in the scene and how far apart they may be. The inputs are the measured distance and the limits for the nearest
             * and the farthest surfaces from the recipe (see Comfort). A SceneComfort entry for the scene (see SceneComfort) overrides them.
             */
            static SceneLimits resolve_scene_limits(const Analysis::Recipe::Recipe& in_recipe,
                                                    const SceneMeasure&             in_measure);

        private:
            /* The class of a draw and whether it is pushed toward the viewer, as a converter that plays an application as it runs sees them. */
            typedef std::pair<Analysis::Recipe::DrawClass, bool>               ClassKey;
            typedef std::map<ClassKey, std::set<uint32_t> >                    ClassMap;
            typedef std::vector<Analysis::Recipe::DrawConvergence>             ConvergenceVector;
            typedef std::vector<std::array<float, 16> >                        MatrixVector;
            typedef std::vector<Analysis::Recipe::PerEyeTexture>               PerEyeTextureVector;
            typedef std::vector<Analysis::Recipe::DrawRule>                    RuleVector;
            typedef std::vector<Analysis::Recipe::SceneConvergence>            SceneConvergenceVector;
            typedef std::vector<Analysis::Recipe::SceneSeparation>             SceneSeparationVector;

            /* The rules for a signature that passes of different classes draw. Each class that is not affected, or is pushed, is selected by the view
             * matrices of its passes. Returns false, and says why, if the matrices do not tell the classes apart.
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

            /* Classifies draws by what the analysis saw them do, whatever the API. Draws in pixel space are layers. Draws that always come after a 3D
             * draw are overlays. Draws at the camera with vertices in front of it are in eye space. A draw that samples a copy of the picture is not
             * an overlay. Keys whose draws disagree are left out.
             */
            static DrawKeyLists get_automatic_layers(const Analysis::APIDumpAnalysis::AnalysisResult& in_analysis);

            /* The keys of the draws that cover the whole view, taken from the passes that the recipe leaves alone. Only those draws are moved by the
             * overlay. The list is limited to them because the mesh of a scene can have the same key.
             */
            static std::vector<uint64_t> get_covering_draw_key_vec(const Analysis::Recipe::Recipe&                  in_recipe,
                                                                   const Analysis::APIDumpAnalysis::AnalysisResult& in_analysis);

            /* The textures that frames carry over from the previous frame and that each eye has its own copy of. The back buffer is not included,
             * because the player already keeps its pictures apart for the eyes. */
            static PerEyeTextureVector get_per_eye_texture_vec(const Analysis::APIDumpAnalysis::AnalysisResult&            in_analysis,
                                                               const Analysis::APIDumpDependencies::DependencyFileContent* in_dependencies_ptr);

            /* The class as it is written in a message: "affected" or "unaffected, pushed". */
            static std::string get_class_key_text(const ClassKey& in_class_key);

            static const char* get_class_name(const Analysis::Recipe::DrawClass& in_class);

            /* A key as it is written in a recipe. */
            static std::string get_key_text(const uint64_t& in_key);

            /* The horizontal scale of the projection of a pass, which is 1 / tan(half the angle of view across the picture width). It is taken from
             * the first value that the camera of the pass had; 0 if the pass has no projection.
             */
            static float get_projection_scale(const Analysis::APIDumpAnalysis::AnalysisResult& in_analysis,
                                              const uint32_t&                                  in_pass_id);

            /* The keys of flat draws that reach from the left edge to the right edge (see DrawFacts). A push widens their viewport so that no edge
             * stays bare. */
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
