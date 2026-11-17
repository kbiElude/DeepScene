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
#include "overlay_candidate_finder.h"
#include <algorithm>
#include <map>
#include <math.h>
#include <set>
#include <stdio.h>
#include <string.h>

bool Analysis::RecipeBake::RecipeBaker::bake(const std::string&                                          in_recipe_text,
                                             const Analysis::APIDumpAnalysis::AnalysisResult&            in_analysis,
                                             std::string*                                                out_text_ptr,
                                             BakeReport*                                                 out_report_ptr,
                                             std::string*                                                out_error_ptr,
                                             const Analysis::APIDumpDependencies::DependencyFileContent* in_dependencies_ptr)
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

    if (root.type != Utils::Json::JsonValueType::OBJECT)
    {
        *out_error_ptr = "The recipe must be a JSON object";

        goto end;
    }

    /* A "baked" section from an earlier baking is of no use, and it may have a version that this build cannot read. */
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

    /* The dependencies must come from the same workload as the analysis. */
    if (in_dependencies_ptr != nullptr)
    {
        const bool is_of_workload = Analysis::APIDumpAnalysis::WorkloadIdentity::is_same_workload(in_analysis.workload_reference,
                                                                                                  in_dependencies_ptr->workload_reference);

        if (is_of_workload == false)
        {
            *out_error_ptr = "The dependencies (the .deps file) are not of the workload that the analysis (the .passes file) is of";

            goto end;
        }
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

        const PerEyeTextureVector per_eye_texture_vec = get_per_eye_texture_vec(in_analysis,
                                                                                in_dependencies_ptr);
        DrawKeyLists              draw_key_lists      = get_automatic_layers   (in_analysis);

        /* Only a recipe that asks for automatic layers gets them, so other recipes bake as before. */
        draw_key_lists.covering_draw_key_vec      = get_covering_draw_key_vec     (recipe,
                                                                                   in_analysis);
        draw_key_lists.has_automatic_layers       = recipe.comfort.uses_automatic_layers;
        draw_key_lists.side_reaching_draw_key_vec = get_side_reaching_draw_key_vec(in_analysis);

        out_report_ptr->n_per_eye_textures = static_cast<uint32_t>(per_eye_texture_vec.size() );
        out_report_ptr->uses_dependencies  = (in_dependencies_ptr != nullptr);

        write_baked(draw_key_lists,
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
    /* More than this many view matrices would make a rule that costs the converter too much time. */
    constexpr size_t MAX_VIEW_MATRICES_PER_RULE = 64;

    /* TODO: the view matrix is the only thing that tells apart the passes that draw a signature differently. An application where it is not
     * enough (the matrices of two classes overlap, or a pass has none, as when the camera is a shader constant) needs the deferred path of
     * the converter: the calls of a pass are held back until the pass is known, and then played.
     */
    const std::string deferred_text = ". This application needs the converter to hold the draws "
                                      "of a pass back until it knows which pass it is, which the "
                                      "converter cannot do";

    std::map<ClassKey, MatrixVector> matrix_vec_by_class;
    bool                             result              = false;

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
                                         get_class_key_text(first_pair.first)            +
                                         " and as "                                      +
                                         get_class_key_text(second_pair.first)           +
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
        /* A draw that no rule applies to is affected and not pushed. */
        if (matrix_pair.first.first  != Analysis::Recipe::DrawClass::AFFECTED ||
            matrix_pair.first.second)
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

            rule.draw_class    = matrix_pair.first.first;
            rule.is_pushed     = matrix_pair.first.second;
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

    const Analysis::Recipe::RecipeApplier applier   (in_recipe);
    std::vector<uint32_t>                 parent_vec(n_passes);
    std::map<uint64_t, uint32_t>                 pass_of_signature_map;

    /* Finds the group of a pass: passes that draw the same signature are in one group, whatever else they draw. */
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

    /* The converter does not know the pass of a draw, only its signature. A distance given per signature, not per pass, therefore has to be
     * one distance for all the signatures of the passes that share any signature. Otherwise the draws of one pass would converge at
     * different distances.
     */
    for (uint32_t n_pass = 0;
                  n_pass < n_passes;
                ++n_pass)
    {
        const auto& structure = model_ref.pass_structure_interner.get_structure(n_pass);

        for (const auto& draw_signature : structure.draw_signature_vec)
        {
            if (applier.get_named_draw_class(structure.key,
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

    /* Merge the histograms over all kinds of frames: a signature that passes of several scenes draw has one distance for all of them. */
    for (const auto& histogram_pair : model_ref.depth_histogram_map)
    {
        group_histogram_map[find_group_func(histogram_pair.first.second)].merge(histogram_pair.second);
    }

    for (const auto& signature_pair : group_of_signature_map)
    {
        const auto histogram_iterator = group_histogram_map.find(signature_pair.second);
        bool       has_depth          = false;

        if (histogram_iterator != group_histogram_map.end() )
        {
            const bool is_histogram_empty = histogram_iterator->second.is_empty();

            has_depth = (is_histogram_empty == false);
        }

        if (has_depth)
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
            const auto draw_class = applier.get_named_draw_class(structure.key,
                                                                 draw_signature.structure_key);
            const bool is_pushed  = applier.is_draw_pushed(structure.key,
                                                           draw_signature.structure_key);

            const ClassKey class_key(draw_class,
                                     is_pushed);

            signature_map[draw_signature.signature_key][class_key].insert(n_pass);
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
            /* Every pass draws the signature the same way, so the rule needs no condition. */
            const ClassKey& class_key = signature_pair.second.begin()->first;

            if (class_key.first != Analysis::Recipe::DrawClass::AFFECTED ||
                class_key.second)
            {
                Analysis::Recipe::DrawRule rule;

                rule.draw_class    = class_key.first;
                rule.is_pushed     = class_key.second;
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

void Analysis::RecipeBake::RecipeBaker::compute_scene_convergence(const Analysis::Recipe::Recipe&                  in_recipe,
                                                                  const Analysis::APIDumpAnalysis::AnalysisResult& in_analysis,
                                                                  SceneConvergenceVector*                          out_scene_convergence_vec_ptr,
                                                                  SceneSeparationVector*                           out_scene_separation_vec_ptr,
                                                                  BakeReport*                                      out_report_ptr)
{
    out_scene_convergence_vec_ptr->clear();
    out_scene_separation_vec_ptr->clear  ();

    for (const auto& measure : measure_scenes(in_recipe,
                                              in_analysis) )
    {
        if (measure.has_depth == false)
        {
            out_report_ptr->n_scenes_without_depth++;

            continue;
        }

        const auto limits = resolve_scene_limits(in_recipe,
                                                 measure);

        Analysis::Recipe::SceneConvergence convergence;

        convergence.distance  = limits.distance;
        convergence.shape_key = measure.shape_key;

        out_scene_convergence_vec_ptr->push_back(convergence);

        if (limits.max_separation > 0.0f)
        {
            Analysis::Recipe::SceneSeparation separation;

            separation.max_separation = limits.max_separation;
            separation.shape_key      = measure.shape_key;

            out_scene_separation_vec_ptr->push_back(separation);
        }
    }

    out_report_ptr->n_scene_distances = static_cast<uint32_t>(out_scene_convergence_vec_ptr->size() );
    out_report_ptr->n_scene_limits    = static_cast<uint32_t>(out_scene_separation_vec_ptr->size() );
}

Analysis::RecipeBake::DrawKeyLists Analysis::RecipeBake::RecipeBaker::get_automatic_layers(const Analysis::APIDumpAnalysis::AnalysisResult& in_analysis)
{
    /* The draws of each key, merged over all structures that have it. */
    const auto   total_map = OverlayCandidateFinder::get_facts_by_cover_key(in_analysis);
    DrawKeyLists result;

    for (const auto& total_pair : total_map)
    {
        const auto& total         = total_pair.second;
        const bool  is_flat_layer = (total.n_draws != 0 &&
                                     total.n_draws_flat_layer == total.n_draws);

        if (is_flat_layer)
        {
            const bool is_over_3d   = (total.n_draws_after_3d_draw == total.n_draws);
            const bool is_on_a_copy = (total.n_draws_sampling_copied_color != 0);

            if (is_over_3d    &&
                is_on_a_copy == false)
            {
                result.overlay_draw_key_vec.push_back(total_pair.first);
            }
            else
            {
                result.flat_layer_draw_key_vec.push_back(total_pair.first);
            }
        }
        else
        if (total.n_draws_with_footprint != 0                        &&
            total.n_draws_eye_space      == total.n_draws_with_footprint)
        {
            result.eye_space_draw_key_vec.push_back(total_pair.first);
        }
    }

    return result;
}

std::string Analysis::RecipeBake::RecipeBaker::get_class_key_text(const ClassKey& in_class_key)
{
    std::string result = get_class_name(in_class_key.first);

    if (in_class_key.second)
    {
        result += ", pushed";
    }

    return result;
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

std::vector<uint64_t> Analysis::RecipeBake::RecipeBaker::get_covering_draw_key_vec(const Analysis::Recipe::Recipe&                  in_recipe,
                                                                                   const Analysis::APIDumpAnalysis::AnalysisResult& in_analysis)
{
    const std::set<uint64_t> unaffected_pass_key_set(in_recipe.unaffected.pass_key_vec.begin(),
                                                     in_recipe.unaffected.pass_key_vec.end  ());
    std::set<uint64_t>       key_set;

    for (const auto& pass_pair : in_analysis.model.covering_draw_key_map)
    {
        const uint64_t pass_key = in_analysis.model.pass_structure_interner.get_structure(pass_pair.first).key;

        /* The passes that the recipe leaves alone are not moved by the overlay. When the vertices are moved sideways (a recipe with screen
         * parallax), a draw that covers the view is not moved in any pass either, because that would leave a strip of the picture bare.
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

Analysis::RecipeBake::RecipeBaker::PerEyeTextureVector Analysis::RecipeBake::RecipeBaker::get_per_eye_texture_vec(const Analysis::APIDumpAnalysis::AnalysisResult&            in_analysis,
                                                                                                                  const Analysis::APIDumpDependencies::DependencyFileContent* in_dependencies_ptr)
{
    PerEyeTextureVector result_vec;

    /* The dependencies record what the frames do with each resource, in order, which the totals of the analysis do not. A target that is read
     * in the frame after the one that rendered to it, and is not rewritten in that frame, is carried over too (for example two targets that a
     * demo uses in turns, each in half of the frames).
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
        /* Without the dependencies, use the totals of the analysis: a resource that a frame reads before it writes it, and writes. */
        for (size_t n_resource = 0;
                    n_resource < in_analysis.model.resource_vec.size();
                  ++n_resource)
        {
            const auto& resource = in_analysis.model.resource_vec.at                         (n_resource);
            const auto  access   = Analysis::APIDumpAnalysis::ResourceAccessTracker::classify(in_analysis.model.resource_access_vec.at(n_resource) );

            /* The back buffer is excluded: the player already keeps what a frame carries over in it apart for the eyes (it reads and writes the pixels). */
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

                if (sample_vec.empty()                     == false         &&
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

std::vector<uint64_t> Analysis::RecipeBake::RecipeBaker::get_side_reaching_draw_key_vec(const Analysis::APIDumpAnalysis::AnalysisResult& in_analysis)
{
    /* How close to an edge (-1 or 1) a vertex must be to count as reaching it. */
    constexpr float EDGE_REACH = 0.99f;

    /* The largest depth variation across a flat draw, as a fraction of its depth. */
    constexpr float FLAT_DEPTH_SPREAD = 0.001f;

    std::set<uint64_t> key_set;

    for (const auto& facts_pair : in_analysis.model.draw_facts_map)
    {
        const auto& facts = facts_pair.second;

        if (facts.n_draws_with_footprint == 0)
        {
            continue;
        }

        const float depth_spread = facts.view_depth_max - facts.view_depth_min;
        const float depth_scale  = std::max(::fabsf(facts.view_depth_max),
                                            1.0f);

        if (facts.x_min       <= -EDGE_REACH                      &&
            facts.x_max       >=  EDGE_REACH                      &&
            depth_spread      <= FLAT_DEPTH_SPREAD * depth_scale)
        {
            key_set.insert(facts_pair.first.second);
        }
    }

    return std::vector<uint64_t>(key_set.begin(),
                                 key_set.end  () );
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

std::vector<Analysis::RecipeBake::SceneMeasure> Analysis::RecipeBake::RecipeBaker::measure_scenes(const Analysis::Recipe::Recipe&                  in_recipe,
                                                                                                  const Analysis::APIDumpAnalysis::AnalysisResult& in_analysis)
{
    constexpr double FRACTION_MEDIAN = 0.5;

    const Analysis::Recipe::RecipeApplier applier(in_recipe);
    const auto&                           model_ref = in_analysis.model;
    const auto                            n_shapes  = model_ref.frame_shape_interner.get_n_structures();
    std::vector<SceneMeasure>             result_vec;

    for (uint32_t n_shape = 0;
                  n_shape < n_shapes;
                ++n_shape)
    {
        Analysis::APIDumpAnalysis::DepthHistogram histogram;
        SceneMeasure                              measure;
        double                                    scale_weight      = 0.0;
        double                                    scale_weighted_sum = 0.0;
        const auto&                               shape            = model_ref.frame_shape_interner.get_structure(n_shape);
        Analysis::APIDumpAnalysis::DepthHistogram surface_histogram;

        /* A pass of a frame counts if the recipe affects any draw of it, and it counts once however often the frame has it. */
        for (const auto& pass_id : std::set<uint32_t>(shape.pass_structure_id_vec.begin(),
                                                      shape.pass_structure_id_vec.end() ))
        {
            const auto& structure          = model_ref.pass_structure_interner.get_structure(pass_id);
            const auto  histogram_iterator = model_ref.depth_histogram_map.find             (Analysis::APIDumpAnalysis::DepthHistogramKey(n_shape,
                                                                                                                                          pass_id) );
            bool        is_affected        = false;

            for (const auto& draw_signature : structure.draw_signature_vec)
            {
                if (is_affected == false)
                {
                    const auto draw_class = applier.get_named_draw_class(structure.key,
                                                                         draw_signature.structure_key);

                    is_affected = (draw_class == Analysis::Recipe::DrawClass::AFFECTED);
                }
            }

            const auto surface_iterator = model_ref.surface_depth_histogram_map.find(Analysis::APIDumpAnalysis::DepthHistogramKey(n_shape,
                                                                                                                                  pass_id) );

            /* A pass whose draws have no origin in front of the camera (its model-view matrix is the identity, so the vertices are in the space of the eye)
             * still has the depths of its vertices.
             */
            if (is_affected                                                      &&
                (histogram_iterator != model_ref.depth_histogram_map.end()       ||
                 surface_iterator   != model_ref.surface_depth_histogram_map.end() ) )
            {
                if (histogram_iterator != model_ref.depth_histogram_map.end() )
                {
                    histogram.merge(histogram_iterator->second);
                }

                if (surface_iterator != model_ref.surface_depth_histogram_map.end() )
                {
                    surface_histogram.merge(surface_iterator->second);
                }

                {
                    const auto scale_iterator = model_ref.projection_scale_map.find(Analysis::APIDumpAnalysis::DepthHistogramKey(n_shape,
                                                                                                                                  pass_id) );

                    if (scale_iterator != model_ref.projection_scale_map.end() )
                    {
                        scale_weight       += static_cast<double>(scale_iterator->second.weight);
                        scale_weighted_sum += static_cast<double>(scale_iterator->second.mean) * static_cast<double>(scale_iterator->second.weight);
                    }
                }

                /* An analysis file that does not record the projection of each draw (an older file) gives the first projection that the pass was seen with. */
                if (measure.projection_scale <= 0.0f)
                {
                    measure.projection_scale = get_projection_scale(in_analysis,
                                                                    pass_id);
                }

                /* A pass whose vertices no matrix moves has no projection to read the angle of view from, so the angle of view comes from the recipe. */
                if (measure.projection_scale             <= 0.0f &&
                    in_recipe.screen_parallax.is_enabled)
                {
                    constexpr float DEGREES_TO_RADIANS = 3.14159265358979323846f / 180.0f;
                    constexpr float HALF               = 0.5f;

                    measure.projection_scale = 1.0f / ::tanf(in_recipe.screen_parallax.field_of_view_degrees * HALF * DEGREES_TO_RADIANS);
                }
            }
        }

        /* If the analysis records the projection of the draws, their weighted average replaces the first projection of the pass. */
        if (scale_weight > 0.0)
        {
            measure.projection_scale = static_cast<float>(scale_weighted_sum / scale_weight);
        }

        measure.shape_id = n_shape;

        if (histogram.is_empty() == false)
        {
            measure.distance   = histogram.get_depth_at_fraction                           (FRACTION_MEDIAN);
            /* There are two estimates: one from the vertices of the draws and one from the origins of the draws. Every draw has an origin; a draw whose
             * vertices are not read (a display list) has no vertices. A surface is as near as the nearer estimate says and as far as the farther one says.
             */
            const double near_fraction  = static_cast<double>(in_recipe.comfort.near_fraction);
            const float  origin_far     = histogram.get_depth_at_fraction        (1.0 - near_fraction);
            const float  origin_near    = histogram.get_depth_at_fraction        (near_fraction);
            const float  surface_far    = surface_histogram.get_depth_at_fraction(1.0 - near_fraction);
            const float  surface_near   = surface_histogram.get_depth_at_fraction(near_fraction);

            measure.far_depth  = std::max(origin_far,
                                          surface_far);
            measure.has_depth  = true;
            measure.near_depth = (surface_near > 0.0f) ? std::min(origin_near,
                                                                  surface_near)
                                                       : origin_near;
            measure.shape_key  = Analysis::APIDumpAnalysis::ModelBuilder::compute_shape_key(model_ref,
                                                                                            n_shape);
        }
        else
        if (surface_histogram.is_empty() == false)
        {
            /* Only the vertices say how deep the scene is. */
            const double near_fraction = static_cast<double>(in_recipe.comfort.near_fraction);

            measure.distance   = surface_histogram.get_depth_at_fraction(FRACTION_MEDIAN);
            measure.far_depth  = surface_histogram.get_depth_at_fraction(1.0 - near_fraction);
            measure.has_depth  = true;
            measure.near_depth = surface_histogram.get_depth_at_fraction(near_fraction);
            measure.shape_key  = Analysis::APIDumpAnalysis::ModelBuilder::compute_shape_key(model_ref,
                                                                                            n_shape);
        }

        result_vec.push_back(measure);
    }

    return result_vec;
}

Analysis::RecipeBake::SceneLimits Analysis::RecipeBake::RecipeBaker::resolve_scene_limits(const Analysis::Recipe::Recipe& in_recipe,
                                                                                          const SceneMeasure&             in_measure)
{
    const Analysis::Recipe::RecipeApplier applier(in_recipe);
    const auto*                           scene_ptr = applier.find_scene_comfort_ptr(in_measure.shape_key);
    const auto                            limits    = Analysis::Recipe::RecipeApplier::get_comfort_limits(in_recipe.comfort,
                                                                                                          scene_ptr,
                                                                                                          in_measure.distance,
                                                                                                          in_measure.near_depth,
                                                                                                          in_measure.far_depth,
                                                                                                          in_measure.projection_scale);
    SceneLimits                           result;

    result.distance       = limits.distance;
    result.is_far_limit   = limits.is_far_limit;
    result.is_overridden  = (scene_ptr != nullptr);
    result.max_separation = limits.max_separation;

    return result;
}

void Analysis::RecipeBake::RecipeBaker::write_baked(const DrawKeyLists&           in_draw_key_lists,
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

    for (const auto& signature_key : in_draw_key_lists.covering_draw_key_vec)
    {
        inout_writer_ptr->write_hex_u64(signature_key);
    }

    inout_writer_ptr->end_array();
    inout_writer_ptr->write_key   ("side_reaching_draws");
    inout_writer_ptr->begin_array ();

    for (const auto& cover_key : in_draw_key_lists.side_reaching_draw_key_vec)
    {
        inout_writer_ptr->write_hex_u64(cover_key);
    }

    inout_writer_ptr->end_array();

    if (in_draw_key_lists.has_automatic_layers)
    {
        const std::pair<const char*, const std::vector<uint64_t>*> list_array[] =
        {
            {"eye_space_draws",  &in_draw_key_lists.eye_space_draw_key_vec },
            {"flat_layer_draws", &in_draw_key_lists.flat_layer_draw_key_vec},
            {"overlay_draws",    &in_draw_key_lists.overlay_draw_key_vec   }
        };

        for (const auto& list : list_array)
        {
            inout_writer_ptr->write_key   (list.first);
            inout_writer_ptr->begin_array ();

            for (const auto& cover_key : *list.second)
            {
                inout_writer_ptr->write_hex_u64(cover_key);
            }

            inout_writer_ptr->end_array();
        }
    }

    inout_writer_ptr->write_key("convergence");
    inout_writer_ptr->begin_array();

    for (const auto& convergence : in_convergence_vec)
    {
        inout_writer_ptr->begin_object();
        inout_writer_ptr->write_key    ("signature");
        inout_writer_ptr->write_hex_u64(convergence.signature_key);
        inout_writer_ptr->write_key    ("distance");

        /* write_float does not print the nine significant digits that a float needs to round-trip, so the value is written as a double. */
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

        /* write_float does not print the nine significant digits that a float needs to round-trip, so the value is written as a double. */
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

        /* write_float does not print the nine significant digits that a float needs to round-trip, so the value is written as a double. */
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

        if (rule.is_pushed)
        {
            inout_writer_ptr->write_key ("pushed");
            inout_writer_ptr->write_bool(true); /* in_value */
        }

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

                /* write_float does not print the nine significant digits that a float needs to round-trip, so the value is written as a double. */
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
    /* A whole number up to this limit is written as an integer. JSON numbers are doubles, which represent these values exactly. */
    constexpr double MAX_WHOLE_NUMBER = 9007199254740992.0;

    switch (in_value.type)
    {
        case Utils::Json::JsonValueType::ARRAY:
        {
            bool is_inline = true;

            for (const auto& item : in_value.item_vec)
            {
                is_inline = is_inline && (item.type == Utils::Json::JsonValueType::NUMBER);
            }

            /* Numbers go on one line (a matrix); everything else goes one element per line (a list of keys). */
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

        case Utils::Json::JsonValueType::BOOL:
        {
            inout_writer_ptr->write_bool(in_value.bool_value);

            break;
        }

        case Utils::Json::JsonValueType::NUMBER:
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

        case Utils::Json::JsonValueType::OBJECT:
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

        case Utils::Json::JsonValueType::STRING:
        {
            inout_writer_ptr->write_string(in_value.string_value);

            break;
        }

        case Utils::Json::JsonValueType::NULL_VALUE:
        default:
        {
            inout_writer_ptr->write_null();

            break;
        }
    }
}
