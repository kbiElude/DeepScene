/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_analysis/model_builder.h"
#include "analysis/api_dump_analysis/passes_format.h"
#include "analysis/api_dump_analysis/resource_access_tracker.h"
#include "analysis/api_dump_analysis/resource_identity.h"
#include "analysis/api_dump_analysis/workload_identity.h"
#include "analysis/api_dump_dependencies/per_eye_candidates.h"
#include "analysis/recipe/recipe_applier.h"
#include "recipe_baker.h"
#include "analysis/recipe/recipe_loader.h"
#include <algorithm>
#include <map>
#include <math.h>
#include <set>
#include <stdio.h>
#include <string.h>

bool Analysis::RecipeBake::RecipeBaker::bake(const std::string&                                       in_recipe_text,
                                             const Analysis::APIDumpAnalysis::AnalysisResult&         in_analysis,
                                             std::string*                                             out_text_ptr,
                                             BakeReport*                                              out_report_ptr,
                                             std::string*                                             out_error_ptr,
                                             const Analysis::APIDumpDependencies::DependencyAnalysis* in_dependencies_ptr)
{
    std::string              authored_text;
    ConvergenceVector        convergence_vec;
    std::string              json_error;
    std::vector<uint64_t>    known_signature_key_vec;
    Analysis::Recipe::Recipe recipe;
    bool                     result                  = false;
    Utils::Json::JsonValue   root;
    RuleVector               rule_vec;
    SceneConvergenceVector   scene_convergence_vec;
    SceneSeparationVector    scene_separation_vec;

    *out_report_ptr = BakeReport();

    const std::string recipe_text = Utils::Json::JsonReader::strip_byte_order_mark(in_recipe_text);

    if (Utils::Json::JsonReader::parse(recipe_text,
                                       &root,
                                       &json_error) == false)
    {
        *out_error_ptr = "Not valid JSON: " + json_error;

        goto end;
    }

    if (root.type != Utils::Json::JsonValue::Type::OBJECT)
    {
        *out_error_ptr = "The recipe must be a JSON object";

        goto end;
    }

    /* A section from an earlier baking is of no use, and may be of a version this build cannot read. */
    for (size_t n_member = 0;
                n_member < root.key_vec.size();
              ++n_member)
    {
        if (root.key_vec.at(n_member) == "baked")
        {
            root.item_vec.erase(root.item_vec.begin() + static_cast<std::ptrdiff_t>(n_member) );
            root.key_vec.erase (root.key_vec.begin () + static_cast<std::ptrdiff_t>(n_member) );

            break;
        }
    }

    {
        Utils::Json::JsonWriter writer(true); /* in_is_pretty */

        write_value(root,
                   &writer);

        authored_text = writer.get_text();
    }

    if (Analysis::Recipe::RecipeLoader::parse(authored_text,
                                              &recipe,
                                              out_error_ptr) == false)
    {
        goto end;
    }

    /* What the dependencies say must be of the workload that the analysis is of. */
    if (in_dependencies_ptr != nullptr                                                                                        &&
        Analysis::APIDumpAnalysis::WorkloadIdentity::is_same_workload(in_analysis.workload_reference,
                                                                      in_dependencies_ptr->workload_reference) == false)
    {
        *out_error_ptr = "The dependencies (the .deps file) are not of the workload that the analysis (the .passes file) is of";

        goto end;
    }

    if (compute_draw_rules(recipe,
                           in_analysis,
                          &known_signature_key_vec,
                          &rule_vec,
                           out_report_ptr,
                           out_error_ptr) == false)
    {
        goto end;
    }

    compute_convergence      (recipe,
                              in_analysis,
                             &convergence_vec,
                              out_report_ptr);
    compute_scene_convergence(recipe,
                              in_analysis,
                             &scene_convergence_vec,
                             &scene_separation_vec,
                              out_report_ptr);

    {
        Utils::Json::JsonWriter writer(true); /* in_is_pretty */

        writer.begin_object();

        for (size_t n_member = 0;
                    n_member < root.key_vec.size();
                  ++n_member)
        {
            writer.write_key(root.key_vec.at(n_member) );

            write_value(root.item_vec.at(n_member),
                       &writer);
        }

        writer.write_key("baked");

        const std::vector<uint64_t> covering_draw_key_vec = get_covering_draw_key_vec(recipe,
                                                                                      in_analysis);
        const PerEyeTextureVector   per_eye_texture_vec   = get_per_eye_texture_vec(in_analysis,
                                                                                    in_dependencies_ptr);

        out_report_ptr->n_per_eye_textures       = static_cast<uint32_t>(per_eye_texture_vec.size() );
        out_report_ptr->uses_dependencies        = (in_dependencies_ptr != nullptr);

        write_baked(covering_draw_key_vec,
                    convergence_vec,
                    known_signature_key_vec,
                    per_eye_texture_vec,
                    rule_vec,
                    scene_convergence_vec,
                    scene_separation_vec,
                   &writer);

        writer.end_object();

        *out_text_ptr = writer.get_text() + "\n";
    }

    result = true;

end:
    return result;
}

bool Analysis::RecipeBake::RecipeBaker::compute_conditional_rules(const Analysis::APIDumpAnalysis::AnalysisResult& in_analysis,
                                                                  const uint64_t&                                  in_signature_key,
                                                                  const ClassMap&                                  in_class_map,
                                                                  RuleVector*                                      inout_rule_vec_ptr,
                                                                  std::string*                                     out_error_ptr)
{
    /* More than this many values make a rule that a converter would spend its time on. */
    constexpr size_t MAX_VIEW_MATRICES_PER_RULE = 64;

    /* TODO: the view matrix is all there is to tell apart the passes which draw a signature differently. An application where it is
     * not enough (the matrices of two classes overlap, a pass has none, as when the camera is a shader constant) needs the deferred
     * path of the converter: the calls of a pass are held back until the pass is known, and then played.
     */
    const std::string deferred_text = ". This application needs the converter to hold the draws "
                                      "of a pass back until it knows which pass it is, which the "
                                      "converter cannot do yet";

    std::map<Analysis::Recipe::DrawClass, MatrixVector> matrix_vec_by_class;
    bool                                                result              = false;

    for (const auto& class_pair : in_class_map)
    {
        for (const auto& pass_id : class_pair.second)
        {
            MatrixVector pass_matrix_vec;
            std::string  reason;

            if (get_view_matrix_vec(in_analysis,
                                    pass_id,
                                   &pass_matrix_vec,
                                   &reason) == false)
            {
                *out_error_ptr = "The signature "                                    +
                                 get_key_text(in_signature_key)                      +
                                 " is drawn differently by different passes, "
                                 "which only the view matrix could tell apart, but " +
                                 reason                                              +
                                 deferred_text;

                goto end;
            }

            for (const auto& matrix : pass_matrix_vec)
            {
                auto& class_matrix_vec = matrix_vec_by_class[class_pair.first];

                if (std::find(class_matrix_vec.begin(),
                              class_matrix_vec.end  (),
                              matrix)                   == class_matrix_vec.end() )
                {
                    class_matrix_vec.push_back(matrix);
                }
            }
        }
    }

    for (const auto& first_pair : matrix_vec_by_class)
    {
        for (const auto& second_pair : matrix_vec_by_class)
        {
            for (const auto& matrix : first_pair.second)
            {
                if (first_pair.first < second_pair.first)
                {
                    const auto matrix_iterator = std::find(second_pair.second.begin(),
                                                           second_pair.second.end  (),
                                                           matrix);

                    if (matrix_iterator != second_pair.second.end() )
                    {
                        *out_error_ptr = "The signature "                                +
                                         get_key_text(in_signature_key)                  +
                                         " is drawn as "                                 +
                                         get_class_name(first_pair.first)                +
                                         " and as "                                      +
                                         get_class_name(second_pair.first)               +
                                         " with the same view matrix, so nothing known "
                                         "when it is issued tells the two apart"         +
                                         deferred_text;

                        goto end;
                    }
                }
            }
        }
    }

    for (const auto& matrix_pair : matrix_vec_by_class)
    {
        /* What no rule applies to is affected. */
        if (matrix_pair.first != Analysis::Recipe::DrawClass::AFFECTED)
        {
            Analysis::Recipe::DrawRule rule;

            if (matrix_pair.second.size() > MAX_VIEW_MATRICES_PER_RULE)
            {
                *out_error_ptr = "The signature "                                       +
                                 get_key_text(in_signature_key)                         +
                                 " is told apart by "                                   +
                                 std::to_string(matrix_pair.second.size() )             +
                                 " view matrices, too many for a converter to compare "
                                 "at each draw"                                         +
                                 deferred_text;

                goto end;
            }

            rule.draw_class    = matrix_pair.first;
            rule.signature_key = in_signature_key;

            rule.view_matrix_vec.assign(matrix_pair.second.begin(),
                                        matrix_pair.second.end() );

            inout_rule_vec_ptr->push_back(rule);
        }
    }

    result = true;

end:
    return result;
}

void Analysis::RecipeBake::RecipeBaker::compute_convergence(const Analysis::Recipe::Recipe&                  in_recipe,
                                                            const Analysis::APIDumpAnalysis::AnalysisResult& in_analysis,
                                                            ConvergenceVector*                               out_convergence_vec_ptr,
                                                            BakeReport*                                      out_report_ptr)
{
    constexpr double FRACTION_MEDIAN = 0.5;

    std::map<uint32_t, Analysis::APIDumpAnalysis::DepthHistogram> group_histogram_map;
    std::map<uint64_t, uint32_t>                                  group_of_signature_map;
    const auto&                                                   model_ref              = in_analysis.model;
    const uint32_t                                                n_passes               = static_cast<uint32_t>(model_ref.pass_structure_interner.get_n_structures() );

    const Analysis::Recipe::RecipeApplier applier              (in_recipe);
    std::vector<uint32_t>                        parent_vec           (n_passes);
    std::map<uint64_t, uint32_t>                 pass_of_signature_map;

    /* A pass is found by the group it is in: passes which draw the same signature are in one, whatever follows. */
    const auto find_group_func = [&parent_vec](const uint32_t& in_pass)
                                 {
                                     uint32_t result = in_pass;

                                     while (parent_vec.at(result) != result)
                                     {
                                         parent_vec.at(result) = parent_vec.at(parent_vec.at(result) );
                                         result                = parent_vec.at(result);
                                     }

                                     return result;
                                 };


    out_convergence_vec_ptr->clear();

    for (uint32_t n_pass = 0;
                  n_pass < n_passes;
                ++n_pass)
    {
        parent_vec.at(n_pass) = n_pass;
    }

    /* The converter does not know the pass of a draw, only its signature; a distance told for each signature, and not for each pass,
     * is one for all the signatures of the passes which share any, or the draws of a pass would converge at different distances.
     */
    for (uint32_t n_pass = 0;
                  n_pass < n_passes;
                ++n_pass)
    {
        const auto& structure = model_ref.pass_structure_interner.get_structure(n_pass);

        for (const auto& draw_signature : structure.draw_signature_vec)
        {
            if (get_draw_class(applier,
                               structure.key,
                               draw_signature.structure_key) == Analysis::Recipe::DrawClass::AFFECTED)
            {
                const auto emplaced = pass_of_signature_map.emplace(draw_signature.signature_key,
                                                                    n_pass);

                if (emplaced.second == false)
                {
                    parent_vec.at(find_group_func(n_pass) ) = find_group_func(emplaced.first->second);
                }
            }
        }
    }

    for (const auto& signature_pair : pass_of_signature_map)
    {
        group_of_signature_map[signature_pair.first] = find_group_func(signature_pair.second);
    }

    /* Whatever kind of frame a pass was in: a signature that passes of several scenes draw has one distance for all of them. */
    for (const auto& histogram_pair : model_ref.depth_histogram_map)
    {
        group_histogram_map[find_group_func(histogram_pair.first.second)].merge(histogram_pair.second);
    }

    for (const auto& signature_pair : group_of_signature_map)
    {
        const auto histogram_iterator = group_histogram_map.find(signature_pair.second);

        if (histogram_iterator != group_histogram_map.end() &&
            histogram_iterator->second.is_empty() == false)
        {
            Analysis::Recipe::DrawConvergence convergence;

            convergence.distance      = histogram_iterator->second.get_depth_at_fraction(FRACTION_MEDIAN);
            convergence.signature_key = signature_pair.first;

            out_convergence_vec_ptr->push_back(convergence);
        }
        else
        {
            out_report_ptr->n_signatures_without_depth++;
        }
    }

    {
        std::set<uint32_t> group_set;

        for (const auto& signature_pair : group_of_signature_map)
        {
            group_set.insert(signature_pair.second);
        }

        out_report_ptr->n_convergence_groups = static_cast<uint32_t>(group_set.size() );
    }

    out_report_ptr->n_convergence_distances = static_cast<uint32_t>(out_convergence_vec_ptr->size() );
}

void Analysis::RecipeBake::RecipeBaker::compute_scene_convergence(const Analysis::Recipe::Recipe&                  in_recipe,
                                                                  const Analysis::APIDumpAnalysis::AnalysisResult& in_analysis,
                                                                  SceneConvergenceVector*                          out_scene_convergence_vec_ptr,
                                                                  SceneSeparationVector*                           out_scene_separation_vec_ptr,
                                                                  BakeReport*                                      out_report_ptr)
{
    constexpr double FRACTION_MEDIAN = 0.5;

    const Analysis::Recipe::RecipeApplier applier(in_recipe);
    const auto& model_ref = in_analysis.model;
    const auto  n_shapes  = model_ref.frame_shape_interner.get_n_structures();

    out_scene_convergence_vec_ptr->clear();
    out_scene_separation_vec_ptr->clear  ();

    for (uint32_t n_shape = 0;
                  n_shape < n_shapes;
                ++n_shape)
    {
        Analysis::APIDumpAnalysis::DepthHistogram histogram;
        float                                     projection_scale  = 0.0f;
        const auto&                               shape             = model_ref.frame_shape_interner.get_structure(n_shape);
        Analysis::APIDumpAnalysis::DepthHistogram surface_histogram;

        /* A pass of a frame counts if the recipe affects any draw of it, and counts once however often a frame has it. */
        for (const auto& pass_id : std::set<uint32_t>(shape.pass_structure_id_vec.begin(),
                                                      shape.pass_structure_id_vec.end() ))
        {
            const auto& structure          = model_ref.pass_structure_interner.get_structure(pass_id);
            const auto  histogram_iterator = model_ref.depth_histogram_map.find             (Analysis::APIDumpAnalysis::DepthHistogramKey(n_shape,
                                                                                                                                          pass_id) );
            bool        is_affected        = false;

            for (const auto& draw_signature : structure.draw_signature_vec)
            {
                is_affected = is_affected || (get_draw_class(applier,
                                                             structure.key,
                                                             draw_signature.structure_key) == Analysis::Recipe::DrawClass::AFFECTED);
            }

            if (is_affected                                                  &&
                histogram_iterator != model_ref.depth_histogram_map.end() )
            {
                histogram.merge(histogram_iterator->second);

                const auto surface_iterator = model_ref.surface_depth_histogram_map.find(Analysis::APIDumpAnalysis::DepthHistogramKey(n_shape,
                                                                                                                                       pass_id) );

                if (surface_iterator != model_ref.surface_depth_histogram_map.end() )
                {
                    surface_histogram.merge(surface_iterator->second);
                }

                if (projection_scale <= 0.0f)
                {
                    projection_scale = get_projection_scale(in_analysis,
                                                            pass_id);
                }

                /* A pass whose vertices no matrix moves has no projection to read the angle of view from: the recipe says it. */
                if (projection_scale                     <= 0.0f &&
                    in_recipe.screen_parallax.is_enabled)
                {
                    constexpr float DEGREES_TO_RADIANS = 3.14159265358979323846f / 180.0f;
                    constexpr float HALF               = 0.5f;

                    projection_scale = 1.0f / ::tanf(in_recipe.screen_parallax.field_of_view_degrees * HALF * DEGREES_TO_RADIANS);
                }
            }
        }

        if (histogram.is_empty() == false)
        {
            Analysis::Recipe::SceneConvergence convergence;

            convergence.distance  = histogram.get_depth_at_fraction                           (FRACTION_MEDIAN);
            convergence.shape_key = Analysis::APIDumpAnalysis::ModelBuilder::compute_shape_key(model_ref,
                                                                                               n_shape);

            out_scene_convergence_vec_ptr->push_back(convergence);

            /* The eyes may be no further apart than lets the nearest surface of the scene be at most max_near_parallax of the width of the picture
             * from where the eyes see it at the screen, which is where the scene converges. A surface at depth z, with the eyes a separation s apart
             * (each half of it from the middle), is at the screen when it is at the distance d, and else s * scale * (1 / z - 1 / d) / 2 of the
             * width of the picture from it; the limit is the s at which that is max_near_parallax for the nearest z.
             */
            const float near_depth = surface_histogram.get_depth_at_fraction(static_cast<double>(in_recipe.comfort.near_fraction));

            if (in_recipe.comfort.max_near_parallax > 0.0f &&
                near_depth                          > 0.0f &&
                near_depth                          < convergence.distance)
            {
                Analysis::Recipe::SceneSeparation separation;
                const float                       scale      = (projection_scale > 0.0f) ? projection_scale
                                                                                         : 1.0f;

                separation.max_separation = 2.0f * in_recipe.comfort.max_near_parallax / (scale * (1.0f / near_depth - 1.0f / convergence.distance));
                separation.shape_key      = convergence.shape_key;

                out_scene_separation_vec_ptr->push_back(separation);
            }
        }
        else
        {
            out_report_ptr->n_scenes_without_depth++;
        }
    }

    out_report_ptr->n_scene_distances = static_cast<uint32_t>(out_scene_convergence_vec_ptr->size() );
    out_report_ptr->n_scene_limits    = static_cast<uint32_t>(out_scene_separation_vec_ptr->size() );
}

bool Analysis::RecipeBake::RecipeBaker::compute_draw_rules(const Analysis::Recipe::Recipe&                  in_recipe,
                                                           const Analysis::APIDumpAnalysis::AnalysisResult& in_analysis,
                                                           std::vector<uint64_t>*                           out_known_signature_key_vec_ptr,
                                                           RuleVector*                                      out_rule_vec_ptr,
                                                           BakeReport*                                      out_report_ptr,
                                                           std::string*                                     out_error_ptr)
{
    typedef std::map<uint64_t, ClassMap> SignatureMap;

    const Analysis::Recipe::RecipeApplier applier(in_recipe);
    const auto&  model_ref     = in_analysis.model;
    bool         result        = false;
    SignatureMap signature_map;

    out_rule_vec_ptr->clear();

    for (uint32_t n_pass = 0;
                  n_pass < model_ref.pass_structure_interner.get_n_structures();
                ++n_pass)
    {
        const auto& structure = model_ref.pass_structure_interner.get_structure(n_pass);

        for (const auto& draw_signature : structure.draw_signature_vec)
        {
            signature_map[draw_signature.signature_key][get_draw_class(applier,
                                                                       structure.key,
                                                                       draw_signature.structure_key)].insert(n_pass);
        }
    }

    out_report_ptr->n_signatures = static_cast<uint32_t>(signature_map.size() );

    out_known_signature_key_vec_ptr->clear();

    for (const auto& signature_pair : signature_map)
    {
        out_known_signature_key_vec_ptr->push_back(signature_pair.first);
    }

    for (const auto& signature_pair : signature_map)
    {
        const size_t n_rules_before = out_rule_vec_ptr->size();

        if (signature_pair.second.size() == 1)
        {
            /* Every pass draws it the same way: the rule needs no condition. */
            if (signature_pair.second.begin()->first != Analysis::Recipe::DrawClass::AFFECTED)
            {
                Analysis::Recipe::DrawRule rule;

                rule.draw_class    = signature_pair.second.begin()->first;
                rule.signature_key = signature_pair.first;

                out_rule_vec_ptr->push_back(rule);
            }
        }
        else
        {
            out_report_ptr->n_ambiguous_signatures++;

            if (compute_conditional_rules(in_analysis,
                                          signature_pair.first,
                                          signature_pair.second,
                                          out_rule_vec_ptr,
                                          out_error_ptr) == false)
            {
                goto end;
            }

            out_report_ptr->n_conditional_rules += static_cast<uint32_t>(out_rule_vec_ptr->size() - n_rules_before);
        }
    }

    out_report_ptr->n_rules = static_cast<uint32_t>(out_rule_vec_ptr->size() );
    result                  = true;

end:
    return result;
}

Analysis::RecipeBake::RecipeBaker::PerEyeTextureVector Analysis::RecipeBake::RecipeBaker::get_per_eye_texture_vec(const Analysis::APIDumpAnalysis::AnalysisResult&         in_analysis,
                                                                                                                  const Analysis::APIDumpDependencies::DependencyAnalysis* in_dependencies_ptr)
{
    PerEyeTextureVector result_vec;

    /* The dependencies know what the frames do with each resource in the order they do it, which the totals of the analysis do not: a target which
     * is read in the frame after the one that rendered to it, and not rewritten in that one, is carried over too (two targets that a demo uses by
     * turns are, each in half of the frames).
     */
    if (in_dependencies_ptr != nullptr)
    {
        for (const auto& candidate : Analysis::APIDumpDependencies::PerEyeCandidates::find(*in_dependencies_ptr) )
        {
            Analysis::APIDumpAnalysis::ResourceIdentity identity;

            if (Analysis::APIDumpAnalysis::ResourceIdentities::find(in_dependencies_ptr->resource_vec,
                                                                    in_dependencies_ptr->resource_vec.at(candidate.n_resource).id,
                                                                   &identity) )
            {
                Analysis::Recipe::PerEyeTexture texture;

                texture.description_key = identity.description_key;
                texture.ordinal         = identity.ordinal;

                result_vec.push_back(texture);
            }
        }

    }
    else
    {
        /* Without them, the totals of the analysis: a resource which a frame reads before it writes it, and writes. */
        for (size_t n_resource = 0;
                    n_resource < in_analysis.model.resource_vec.size();
                  ++n_resource)
        {
            const auto& resource = in_analysis.model.resource_vec.at(n_resource);
            const auto  access   = Analysis::APIDumpAnalysis::ResourceAccessTracker::classify(in_analysis.model.resource_access_vec.at(n_resource) );

            /* What a frame carries over in the back buffer is kept apart for the eyes by the player (it reads and writes the pixels). */
            if (access                    == Analysis::APIDumpAnalysis::ResourceAccessClass::LOOP_CARRIED &&
                resource.description.kind != Analysis::APIDumpAnalysis::ResourceKind::SWAP_CHAIN_IMAGE)
            {
                Analysis::APIDumpAnalysis::ResourceIdentity identity;

                if (Analysis::APIDumpAnalysis::ResourceIdentities::find(in_analysis.model.resource_vec,
                                                                        resource.id,
                                                                       &identity) )
                {
                    Analysis::Recipe::PerEyeTexture texture;

                    texture.description_key = identity.description_key;
                    texture.ordinal         = identity.ordinal;

                    result_vec.push_back(texture);
                }
            }
        }
    }

    return result_vec;
}

std::vector<uint64_t> Analysis::RecipeBake::RecipeBaker::get_covering_draw_key_vec(const Analysis::Recipe::Recipe&                  in_recipe,
                                                                                   const Analysis::APIDumpAnalysis::AnalysisResult& in_analysis)
{
    const std::set<uint64_t> unaffected_pass_key_set(in_recipe.unaffected.pass_key_vec.begin(),
                                                     in_recipe.unaffected.pass_key_vec.end  () );
    std::set<uint64_t>       key_set;

    for (const auto& pass_pair : in_analysis.model.covering_draw_key_map)
    {
        const uint64_t pass_key = in_analysis.model.pass_structure_interner.get_structure(pass_pair.first).key;

        /* Those of the passes that the recipe leaves alone are not moved by the overlay; and where the vertices are moved sideways (a recipe that
         * has screen parallax), a draw that covers the view is not, in any pass, for it would leave a strip bare.
         */
        if (unaffected_pass_key_set.count(pass_key) != 0 ||
            in_recipe.screen_parallax.is_enabled)
        {
            key_set.insert(pass_pair.second.begin(),
                           pass_pair.second.end  () );
        }
    }

    return std::vector<uint64_t>(key_set.begin(),
                                 key_set.end  () );
}

const char* Analysis::RecipeBake::RecipeBaker::get_class_name(const Analysis::Recipe::DrawClass& in_class)
{
    const char* result_ptr = "affected";

    switch (in_class)
    {
        case Analysis::Recipe::DrawClass::SKIPPED:
        {
            result_ptr = "skipped";

            break;
        }

        case Analysis::Recipe::DrawClass::UNAFFECTED:
        {
            result_ptr = "unaffected";

            break;
        }

        case Analysis::Recipe::DrawClass::AFFECTED:
        default:
        {
            break;
        }
    }

    return result_ptr;
}

Analysis::Recipe::DrawClass Analysis::RecipeBake::RecipeBaker::get_draw_class(const Analysis::Recipe::RecipeApplier& in_applier,
                                                                              const uint64_t&                        in_pass_key,
                                                                              const uint64_t&                        in_structure_key)
{
    Analysis::Recipe::DrawClass result = Analysis::Recipe::DrawClass::UNAFFECTED;

    if (in_applier.is_draw_skipped(in_pass_key,
                                   in_structure_key) )
    {
        result = Analysis::Recipe::DrawClass::SKIPPED;
    }
    else
    if (in_applier.is_draw_affected(in_pass_key,
                                    in_structure_key) )
    {
        result = Analysis::Recipe::DrawClass::AFFECTED;
    }

    return result;
}

std::string Analysis::RecipeBake::RecipeBaker::get_key_text(const uint64_t& in_key)
{
    constexpr size_t N_BUFFER_BYTES = 32;

    char buffer_i8vec32[N_BUFFER_BYTES];

    ::snprintf(buffer_i8vec32,
               sizeof(buffer_i8vec32),
               "0x%016llx",
               static_cast<unsigned long long>(in_key) );

    return buffer_i8vec32;
}

float Analysis::RecipeBake::RecipeBaker::get_projection_scale(const Analysis::APIDumpAnalysis::AnalysisResult& in_analysis,
                                                              const uint32_t&                                  in_pass_id)
{
    float       result    = 0.0f;
    const auto& model_ref = in_analysis.model;
    const auto& structure = model_ref.pass_structure_interner.get_structure(in_pass_id);

    for (uint32_t n_constant = 0;
                  n_constant < structure.constant_signature_vec.size();
                ++n_constant)
    {
        if (structure.constant_signature_vec.at(n_constant).semantic == Analysis::APIDumpAnalysis::ConstantSemantic::PROJECTION_MATRIX)
        {
            const auto series_iterator = model_ref.constant_series_map.find(Analysis::APIDumpAnalysis::ConstantSeriesKey(in_pass_id,
                                                                                                                         n_constant) );

            if (series_iterator != model_ref.constant_series_map.end() )
            {
                const auto& sample_vec = series_iterator->second.get_sample_vec();

                if (sample_vec.empty() == false                           &&
                    sample_vec.front().value_u8_vec.size() >= sizeof(float) )
                {
                    ::memcpy(&result,
                             sample_vec.front().value_u8_vec.data(),
                             sizeof(float) );
                }
            }
        }
    }

    return result;
}

bool Analysis::RecipeBake::RecipeBaker::get_view_matrix_vec(const Analysis::APIDumpAnalysis::AnalysisResult& in_analysis,
                                                            const uint32_t&                                  in_pass_id,
                                                            MatrixVector*                                    out_matrix_vec_ptr,
                                                            std::string*                                     out_reason_ptr)
{
    constexpr size_t MATRIX_N_BYTES = sizeof(std::array<float, 16>);

    uint32_t    constant_index = 0;
    uint32_t    n_matrices     = 0;
    bool        result         = false;
    const auto& structure      = in_analysis.model.pass_structure_interner.get_structure(in_pass_id);

    out_matrix_vec_ptr->clear();

    for (uint32_t n_constant = 0;
                  n_constant < structure.constant_signature_vec.size();
                ++n_constant)
    {
        if (structure.constant_signature_vec.at(n_constant).semantic == Analysis::APIDumpAnalysis::ConstantSemantic::VIEW_MATRIX)
        {
            constant_index = n_constant;

            n_matrices++;
        }
    }

    if (n_matrices != 1)
    {
        *out_reason_ptr = "pass "                                       +
                          std::to_string(in_pass_id)                    +
                          " has "                                       +
                          ((n_matrices == 0) ? "no view matrix"
                                             : "several view matrices");

        goto end;
    }

    {
        const auto series_iterator = in_analysis.model.constant_series_map.find(Analysis::APIDumpAnalysis::ConstantSeriesKey(in_pass_id,
                                                                                                                             constant_index) );

        if (series_iterator == in_analysis.model.constant_series_map.end() )
        {
            *out_reason_ptr = "the analysis has no values of the view matrix of pass " +
                              std::to_string(in_pass_id);

            goto end;
        }

        for (const auto& sample : series_iterator->second.get_sample_vec() )
        {
            std::array<float, 16> matrix = {};

            if (sample.value_u8_vec.size() != MATRIX_N_BYTES)
            {
                *out_reason_ptr = "the view matrix of pass " +
                                  std::to_string(in_pass_id) +
                                  " is not 16 floats";

                goto end;
            }

            ::memcpy(matrix.data(),
                     sample.value_u8_vec.data(),
                     MATRIX_N_BYTES);

            out_matrix_vec_ptr->push_back(matrix);
        }
    }

    result = true;

end:
    return result;
}

void Analysis::RecipeBake::RecipeBaker::write_baked(const std::vector<uint64_t>&  in_covering_draw_key_vec,
                                                    const ConvergenceVector&      in_convergence_vec,
                                                    const std::vector<uint64_t>&  in_known_signature_key_vec,
                                                    const PerEyeTextureVector&    in_per_eye_texture_vec,
                                                    const RuleVector&             in_rule_vec,
                                                    const SceneConvergenceVector& in_scene_convergence_vec,
                                                    const SceneSeparationVector&  in_scene_separation_vec,
                                                    Utils::Json::JsonWriter*      inout_writer_ptr)
{
    inout_writer_ptr->begin_object();
    inout_writer_ptr->write_key   ("signature_scheme_version");
    inout_writer_ptr->write_u64   (Analysis::APIDumpAnalysis::PassesFormat::SIGNATURE_SCHEME_VERSION);
    inout_writer_ptr->write_key   ("known_signatures");
    inout_writer_ptr->begin_array ();

    for (const auto& signature_key : in_known_signature_key_vec)
    {
        inout_writer_ptr->write_hex_u64(signature_key);
    }

    inout_writer_ptr->end_array();
    inout_writer_ptr->write_key   ("per_eye_textures");
    inout_writer_ptr->begin_array ();

    for (const auto& texture : in_per_eye_texture_vec)
    {
        inout_writer_ptr->begin_object();
        inout_writer_ptr->write_key    ("description_key");
        inout_writer_ptr->write_hex_u64(texture.description_key);
        inout_writer_ptr->write_key    ("ordinal");
        inout_writer_ptr->write_u64    (texture.ordinal);
        inout_writer_ptr->end_object();
    }

    inout_writer_ptr->end_array();
    inout_writer_ptr->write_key   ("covering_draws");
    inout_writer_ptr->begin_array ();

    for (const auto& signature_key : in_covering_draw_key_vec)
    {
        inout_writer_ptr->write_hex_u64(signature_key);
    }

    inout_writer_ptr->end_array();
    inout_writer_ptr->write_key("convergence");
    inout_writer_ptr->begin_array();

    for (const auto& convergence : in_convergence_vec)
    {
        inout_writer_ptr->begin_object();
        inout_writer_ptr->write_key    ("signature");
        inout_writer_ptr->write_hex_u64(convergence.signature_key);
        inout_writer_ptr->write_key    ("distance");

        /* A float is exact in nine digits, which is what write_float does not give. */
        inout_writer_ptr->write_double (static_cast<double>(convergence.distance) );
        inout_writer_ptr->end_object();
    }

    inout_writer_ptr->end_array();
    inout_writer_ptr->write_key("scene_convergence");
    inout_writer_ptr->begin_array();

    for (const auto& scene_convergence : in_scene_convergence_vec)
    {
        inout_writer_ptr->begin_object();
        inout_writer_ptr->write_key    ("shape");
        inout_writer_ptr->write_hex_u64(scene_convergence.shape_key);
        inout_writer_ptr->write_key    ("distance");

        /* A float is exact in nine digits, which is what write_float does not give. */
        inout_writer_ptr->write_double (static_cast<double>(scene_convergence.distance) );
        inout_writer_ptr->end_object();
    }

    inout_writer_ptr->end_array();
    inout_writer_ptr->write_key("scene_max_separation");
    inout_writer_ptr->begin_array();

    for (const auto& scene_separation : in_scene_separation_vec)
    {
        inout_writer_ptr->begin_object();
        inout_writer_ptr->write_key    ("shape");
        inout_writer_ptr->write_hex_u64(scene_separation.shape_key);
        inout_writer_ptr->write_key    ("separation");

        /* A float is exact in nine digits, which is what write_float does not give. */
        inout_writer_ptr->write_double (static_cast<double>(scene_separation.max_separation) );
        inout_writer_ptr->end_object();
    }

    inout_writer_ptr->end_array();
    inout_writer_ptr->write_key("draw_rules");
    inout_writer_ptr->begin_array();

    for (const auto& rule : in_rule_vec)
    {
        inout_writer_ptr->begin_object();
        inout_writer_ptr->write_key    ("signature");
        inout_writer_ptr->write_hex_u64(rule.signature_key);
        inout_writer_ptr->write_key    ("class");
        inout_writer_ptr->write_string (get_class_name(rule.draw_class) );

        if (rule.view_matrix_vec.empty() == false)
        {
            inout_writer_ptr->write_key   ("when");
            inout_writer_ptr->begin_object();
            inout_writer_ptr->write_key   ("constant");
            inout_writer_ptr->write_string("view_matrix");
            inout_writer_ptr->write_key   ("equals_any");
            inout_writer_ptr->begin_array ();

            for (const auto& matrix : rule.view_matrix_vec)
            {
                inout_writer_ptr->begin_inline_array();

                /* A float is exact in nine digits, which is what write_float does not give. */
                for (const auto& element : matrix)
                {
                    inout_writer_ptr->write_double(static_cast<double>(element) );
                }

                inout_writer_ptr->end_array();
            }

            inout_writer_ptr->end_array ();
            inout_writer_ptr->end_object();
        }

        inout_writer_ptr->end_object();
    }

    inout_writer_ptr->end_array ();
    inout_writer_ptr->end_object();
}

void Analysis::RecipeBake::RecipeBaker::write_value(const Utils::Json::JsonValue& in_value,
                                                    Utils::Json::JsonWriter*      inout_writer_ptr)
{
    /* A whole number up to here is written as one; JSON numbers are doubles, which hold these exactly. */
    constexpr double MAX_WHOLE_NUMBER = 9007199254740992.0;

    switch (in_value.type)
    {
        case Utils::Json::JsonValue::Type::ARRAY:
        {
            bool is_inline = true;

            for (const auto& item : in_value.item_vec)
            {
                is_inline = is_inline && (item.type == Utils::Json::JsonValue::Type::NUMBER);
            }

            /* Numbers go on one line (a matrix), the rest one to a line (a list of keys). */
            if (is_inline)
            {
                inout_writer_ptr->begin_inline_array();
            }
            else
            {
                inout_writer_ptr->begin_array();
            }

            for (const auto& item : in_value.item_vec)
            {
                write_value(item,
                            inout_writer_ptr);
            }

            inout_writer_ptr->end_array();

            break;
        }

        case Utils::Json::JsonValue::Type::BOOL:
        {
            inout_writer_ptr->write_bool(in_value.bool_value);

            break;
        }

        case Utils::Json::JsonValue::Type::NUMBER:
        {
            const double absolute_value = ::fabs(in_value.number_value);
            bool         is_whole       = false;

            if (absolute_value < MAX_WHOLE_NUMBER)
            {
                const double floor_value = ::floor(in_value.number_value);

                is_whole = (in_value.number_value == floor_value);
            }

            if (is_whole)
            {
                inout_writer_ptr->write_i64(static_cast<int64_t>(in_value.number_value) );
            }
            else
            {
                inout_writer_ptr->write_double(in_value.number_value);
            }

            break;
        }

        case Utils::Json::JsonValue::Type::OBJECT:
        {
            inout_writer_ptr->begin_object();

            for (size_t n_member = 0;
                        n_member < in_value.key_vec.size();
                      ++n_member)
            {
                inout_writer_ptr->write_key(in_value.key_vec.at(n_member) );

                write_value(in_value.item_vec.at(n_member),
                            inout_writer_ptr);
            }

            inout_writer_ptr->end_object();

            break;
        }

        case Utils::Json::JsonValue::Type::STRING:
        {
            inout_writer_ptr->write_string(in_value.string_value);

            break;
        }

        case Utils::Json::JsonValue::Type::NULL_VALUE:
        default:
        {
            inout_writer_ptr->write_null();

            break;
        }
    }
}
