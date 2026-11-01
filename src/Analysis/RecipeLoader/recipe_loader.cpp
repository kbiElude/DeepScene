/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "Analysis/APIDumpAnalysis/passes_format.h"
#include "Analysis/RecipeLoader/recipe_loader.h"
#include "Interception/Hooks/D3D8/types.h"
#include <algorithm>
#include <ctype.h>
#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <utility>

bool Analysis::RecipeLoader::RecipeLoader::check_keys(const Analysis::APIDumpAnalysis::JsonValue& in_object,
                                                      const std::vector<std::string>&             in_allowed_key_vec,
                                                      const std::string&                          in_path,
                                                      std::string*                                out_error_ptr)
{
    const std::string* unknown_key_ptr = in_object.find_unknown_key_ptr(in_allowed_key_vec);

    if (unknown_key_ptr != nullptr)
    {
        *out_error_ptr = ((in_path.empty() ) ? std::string()
                                             : in_path + ": ") + "unknown field \"" + *unknown_key_ptr + "\"";
    }

    return (unknown_key_ptr == nullptr);
}

const Analysis::RecipeLoader::RecipeLoader::CallDescriptor* Analysis::RecipeLoader::RecipeLoader::find_call_descriptor_ptr(const std::string& in_name)
{
    /* The functions a rule can apply to: name, function and argument positions (argument 0 is the object it is called on). */
    static const CallDescriptor CALL_DESCRIPTOR_ARRAY[] =
    {
        {APIInterceptor::APIFunction::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_SETTRANSFORM, 2, "IDirect3DDevice8::SetTransform", 1}
    };

    const CallDescriptor* result_ptr = nullptr;

    for (const auto& descriptor : CALL_DESCRIPTOR_ARRAY)
    {
        if (in_name == descriptor.name)
        {
            result_ptr = &descriptor;
        }
    }

    return result_ptr;
}

bool Analysis::RecipeLoader::RecipeLoader::has_common_key(const std::vector<uint64_t>& in_a,
                                                          const std::vector<uint64_t>& in_b)
{
    bool result = false;

    for (const auto& key : in_a)
    {
        if (std::find(in_b.begin(),
                      in_b.end  (),
                      key) != in_b.end() )
        {
            result = true;
        }
    }

    return result;
}

bool Analysis::RecipeLoader::RecipeLoader::load(const std::string& in_filename,
                                                Recipe*            out_recipe_ptr,
                                                std::string*       out_error_ptr)
{
    bool        result = false;
    std::string text;

    if (Analysis::APIDumpAnalysis::JsonReader::read_file(in_filename,
                                                        &text) == false)
    {
        *out_error_ptr = "Cannot open " + in_filename;

        goto end;
    }

    result = parse(text,
                   out_recipe_ptr,
                   out_error_ptr);

end:
    return result;
}

bool Analysis::RecipeLoader::RecipeLoader::parse(const std::string& in_text,
                                                 Recipe*            out_recipe_ptr,
                                                 std::string*       out_error_ptr)
{
    constexpr uint32_t SUPPORTED_RECIPE_VERSION = 0;

    std::string                          json_error;
    Analysis::APIDumpAnalysis::JsonValue root;
    bool                                 result = false;

    *out_recipe_ptr = Recipe();

    const std::string text = Analysis::APIDumpAnalysis::JsonReader::strip_byte_order_mark(in_text);

    if (Analysis::APIDumpAnalysis::JsonReader::parse(text,
                                                    &root,
                                                    &json_error) == false)
    {
        *out_error_ptr = "Not valid JSON: " + json_error;

        goto end;
    }

    if (root.type != Analysis::APIDumpAnalysis::JsonValue::Type::OBJECT)
    {
        *out_error_ptr = "The recipe must be a JSON object";

        goto end;
    }

    if (check_keys(root,
                   {"affected", "baked", "comfort", "key_scheme_version", "known_shape_keys", "on_unknown_frame", "recipe_version", "rules", "skipped", "target", "unaffected"},
                   "",
                   out_error_ptr) == false)
    {
        goto end;
    }

    {
        const auto* affected_ptr           = root.find_ptr("affected");
        const auto* baked_ptr              = root.find_ptr("baked");
        const auto* comfort_ptr            = root.find_ptr("comfort");
        const auto* key_scheme_version_ptr = root.find_ptr("key_scheme_version");
        const auto* known_shape_keys_ptr   = root.find_ptr("known_shape_keys");
        const auto* on_unknown_frame_ptr   = root.find_ptr("on_unknown_frame");
        const auto* recipe_version_ptr     = root.find_ptr("recipe_version");
        const auto* rule_array_ptr         = root.find_ptr("rules");
        const auto* skipped_ptr            = root.find_ptr("skipped");
        const auto* target_ptr             = root.find_ptr("target");
        const auto* unaffected_ptr         = root.find_ptr("unaffected");

        if (recipe_version_ptr == nullptr)
        {
            *out_error_ptr = "recipe_version is missing";

            goto end;
        }

        if (parse_unsigned(*recipe_version_ptr,
                           "recipe_version",
                          &out_recipe_ptr->version,
                           out_error_ptr) == false)
        {
            goto end;
        }

        if (out_recipe_ptr->version != SUPPORTED_RECIPE_VERSION)
        {
            *out_error_ptr = "recipe_version: this version reads recipes of version " + std::to_string(SUPPORTED_RECIPE_VERSION) +
                             ", not " + std::to_string(out_recipe_ptr->version);

            goto end;
        }

        if (key_scheme_version_ptr == nullptr)
        {
            *out_error_ptr = "key_scheme_version is missing";

            goto end;
        }

        if (parse_unsigned(*key_scheme_version_ptr,
                           "key_scheme_version",
                          &out_recipe_ptr->key_scheme_version,
                           out_error_ptr) == false)
        {
            goto end;
        }

        if (out_recipe_ptr->key_scheme_version != Analysis::APIDumpAnalysis::PassesFormat::KEY_SCHEME_VERSION)
        {
            *out_error_ptr = "key_scheme_version: the keys of version " + std::to_string(out_recipe_ptr->key_scheme_version) +
                             " are not the keys of this build (version " +
                             std::to_string(Analysis::APIDumpAnalysis::PassesFormat::KEY_SCHEME_VERSION) + ")";

            goto end;
        }

        if (target_ptr != nullptr)
        {
            if (target_ptr->type != Analysis::APIDumpAnalysis::JsonValue::Type::OBJECT)
            {
                *out_error_ptr = "target: expected an object";

                goto end;
            }

            if (check_keys(*target_ptr,
                           {"description", "name"},
                           "target",
                           out_error_ptr) == false)
            {
                goto end;
            }

            if (target_ptr->at("description").type != Analysis::APIDumpAnalysis::JsonValue::Type::NULL_VALUE)
            {
                if (target_ptr->at("description").type != Analysis::APIDumpAnalysis::JsonValue::Type::STRING)
                {
                    *out_error_ptr = "target.description: expected a string";

                    goto end;
                }

                out_recipe_ptr->target_description = target_ptr->at("description").string_value;
            }

            if (target_ptr->at("name").type != Analysis::APIDumpAnalysis::JsonValue::Type::NULL_VALUE)
            {
                if (target_ptr->at("name").type != Analysis::APIDumpAnalysis::JsonValue::Type::STRING)
                {
                    *out_error_ptr = "target.name: expected a string";

                    goto end;
                }

                out_recipe_ptr->target_name = target_ptr->at("name").string_value;
            }
        }

        if (known_shape_keys_ptr != nullptr)
        {
            if (parse_hex_key_array(*known_shape_keys_ptr,
                                    "known_shape_keys",
                                   &out_recipe_ptr->known_shape_key_vec,
                                    out_error_ptr) == false)
            {
                goto end;
            }

            /* No list at all is how a recipe says that every frame is known. */
            if (out_recipe_ptr->known_shape_key_vec.empty() )
            {
                *out_error_ptr = "known_shape_keys: the list is empty, so no frame would be known; leave the field out to know every frame";

                goto end;
            }
        }

        if (affected_ptr != nullptr)
        {
            if (parse_draw_selector(*affected_ptr,
                                    "affected",
                                   &out_recipe_ptr->affected,
                                    out_error_ptr) == false)
            {
                goto end;
            }
        }

        if (skipped_ptr != nullptr)
        {
            if (parse_draw_selector(*skipped_ptr,
                                    "skipped",
                                   &out_recipe_ptr->skipped,
                                    out_error_ptr) == false)
            {
                goto end;
            }
        }

        if (unaffected_ptr != nullptr)
        {
            if (parse_draw_selector(*unaffected_ptr,
                                    "unaffected",
                                   &out_recipe_ptr->unaffected,
                                    out_error_ptr) == false)
            {
                goto end;
            }
        }

        if (comfort_ptr != nullptr)
        {
            if (parse_comfort(*comfort_ptr,
                              "comfort",
                             &out_recipe_ptr->comfort,
                              out_error_ptr) == false)
            {
                goto end;
            }
        }

        if (baked_ptr != nullptr)
        {
            if (parse_baked(*baked_ptr,
                            "baked",
                            out_recipe_ptr,
                            out_error_ptr) == false)
            {
                goto end;
            }
        }

        const bool has_common_draw_key = has_common_key(out_recipe_ptr->affected.draw_key_vec,
                                                        out_recipe_ptr->unaffected.draw_key_vec);
        const bool has_common_pass_key = has_common_key(out_recipe_ptr->affected.pass_key_vec,
                                                        out_recipe_ptr->unaffected.pass_key_vec);

        if (has_common_draw_key ||
            has_common_pass_key)
        {
            *out_error_ptr = "affected and unaffected: a key is in both";

            goto end;
        }


        if (on_unknown_frame_ptr != nullptr)
        {
            if (on_unknown_frame_ptr->type != Analysis::APIDumpAnalysis::JsonValue::Type::STRING)
            {
                *out_error_ptr = "on_unknown_frame: expected \"mono\", \"skip\" or \"fail\"";

                goto end;
            }

            if (on_unknown_frame_ptr->string_value == "fail")
            {
                out_recipe_ptr->unknown_frame_policy = UnknownFramePolicy::FAIL;
            }
            else
            if (on_unknown_frame_ptr->string_value == "mono")
            {
                out_recipe_ptr->unknown_frame_policy = UnknownFramePolicy::MONO;
            }
            else
            if (on_unknown_frame_ptr->string_value == "skip")
            {
                out_recipe_ptr->unknown_frame_policy = UnknownFramePolicy::SKIP;
            }
            else
            {
                *out_error_ptr = "on_unknown_frame: expected \"mono\", \"skip\" or \"fail\", not \"" +
                                 on_unknown_frame_ptr->string_value + "\"";

                goto end;
            }
        }

        if (rule_array_ptr == nullptr)
        {
            *out_error_ptr = "rules is missing";

            goto end;
        }

        if (rule_array_ptr->type != Analysis::APIDumpAnalysis::JsonValue::Type::ARRAY)
        {
            *out_error_ptr = "rules: expected an array";

            goto end;
        }

        for (size_t n_rule = 0;
                    n_rule < rule_array_ptr->item_vec.size();
                  ++n_rule)
        {
            Rule rule;

            if (parse_rule(rule_array_ptr->item_vec.at(n_rule),
                           "rules[" + std::to_string(n_rule) + "]",
                          &rule,
                           out_error_ptr) == false)
            {
                goto end;
            }

            for (const auto& other_rule : out_recipe_ptr->rule_vec)
            {
                if (rule.name.empty() == false &&
                    rule.name        == other_rule.name)
                {
                    *out_error_ptr = "rules[" + std::to_string(n_rule) + "].name: \"" + rule.name + "\" is used by another rule";

                    goto end;
                }
            }

            out_recipe_ptr->rule_vec.push_back(rule);
        }
    }

    result = true;

end:
    return result;
}

bool Analysis::RecipeLoader::RecipeLoader::parse_action(const Analysis::APIDumpAnalysis::JsonValue& in_action,
                                                        const std::string&                          in_path,
                                                        Rule*                                       inout_rule_ptr,
                                                        std::string*                                out_error_ptr)
{
    bool result = false;

    if (in_action.type != Analysis::APIDumpAnalysis::JsonValue::Type::OBJECT)
    {
        *out_error_ptr = in_path + ": expected an object";

        goto end;
    }

    if (check_keys(in_action,
                   {"axis", "left", "order", "right", "type"},
                   in_path,
                   out_error_ptr) == false)
    {
        goto end;
    }

    {
        const auto* order_ptr = in_action.find_ptr("order");
        const auto* type_ptr  = in_action.find_ptr("type");

        if (type_ptr == nullptr)
        {
            *out_error_ptr = in_path + ".type is missing";

            goto end;
        }

        if (type_ptr->type         != Analysis::APIDumpAnalysis::JsonValue::Type::STRING ||
            (type_ptr->string_value != "eye_convergence"                                 &&
             type_ptr->string_value != "eye_offset"                                      &&
             type_ptr->string_value != "multiply"                                        &&
             type_ptr->string_value != "replace"))
        {
            *out_error_ptr = in_path + ".type: expected \"eye_convergence\", \"eye_offset\", \"multiply\" or \"replace\"";

            goto end;
        }

        inout_rule_ptr->action_kind = (type_ptr->string_value == "eye_convergence") ? ActionKind::EYE_CONVERGENCE
                                    : (type_ptr->string_value == "eye_offset")      ? ActionKind::EYE_OFFSET
                                    : (type_ptr->string_value == "multiply")        ? ActionKind::MULTIPLY
                                                                                    : ActionKind::REPLACE;

        if (inout_rule_ptr->action_kind != ActionKind::EYE_OFFSET &&
            in_action.find_ptr("axis")  != nullptr)
        {
            *out_error_ptr = in_path + ".axis: only eye_offset has an axis";

            goto end;
        }

        if (inout_rule_ptr->action_kind == ActionKind::EYE_CONVERGENCE)
        {
            if (order_ptr                  != nullptr ||
                in_action.find_ptr("left")  != nullptr ||
                in_action.find_ptr("right") != nullptr)
            {
                *out_error_ptr = in_path + ": eye_convergence takes no order or matrices (order, left, right)";

                goto end;
            }

            result = true;

            goto end;
        }

        if (inout_rule_ptr->action_kind != ActionKind::REPLACE)
        {
            if (order_ptr == nullptr)
            {
                *out_error_ptr = in_path + ".order is missing (it needs \"post\" or \"pre\")";

                goto end;
            }

            if (order_ptr->type         != Analysis::APIDumpAnalysis::JsonValue::Type::STRING ||
                (order_ptr->string_value != "post"                                            &&
                 order_ptr->string_value != "pre"))
            {
                *out_error_ptr = in_path + ".order: expected \"post\" or \"pre\"";

                goto end;
            }

            inout_rule_ptr->multiply_order = (order_ptr->string_value == "post") ? MultiplyOrder::POST
                                                                                 : MultiplyOrder::PRE;
        }
        else
        if (order_ptr != nullptr)
        {
            *out_error_ptr = in_path + ".order: replace has no order";

            goto end;
        }

        if (inout_rule_ptr->action_kind == ActionKind::EYE_OFFSET)
        {
            const auto* axis_ptr = in_action.find_ptr("axis");

            if (axis_ptr == nullptr)
            {
                *out_error_ptr = in_path + ".axis is missing (eye_offset needs \"x\", \"y\" or \"z\")";

                goto end;
            }

            if (axis_ptr->type         != Analysis::APIDumpAnalysis::JsonValue::Type::STRING ||
                (axis_ptr->string_value != "x"                                               &&
                 axis_ptr->string_value != "y"                                               &&
                 axis_ptr->string_value != "z"))
            {
                *out_error_ptr = in_path + ".axis: expected \"x\", \"y\" or \"z\"";

                goto end;
            }

            inout_rule_ptr->offset_axis = (axis_ptr->string_value == "x") ? Axis::X
                                        : (axis_ptr->string_value == "y") ? Axis::Y
                                                                          : Axis::Z;

            if (in_action.find_ptr("left")  != nullptr ||
                in_action.find_ptr("right") != nullptr)
            {
                *out_error_ptr = in_path + ": eye_offset takes no matrices (left, right)";

                goto end;
            }

            result = true;

            goto end;
        }

        if (in_action.find_ptr("left") == nullptr)
        {
            *out_error_ptr = in_path + ".left is missing";

            goto end;
        }

        if (in_action.find_ptr("right") == nullptr)
        {
            *out_error_ptr = in_path + ".right is missing";

            goto end;
        }

        if (parse_matrix(in_action.at("left"),
                         in_path + ".left",
                        &inout_rule_ptr->left_matrix_fp32vec16,
                         out_error_ptr) == false)
        {
            goto end;
        }

        if (parse_matrix(in_action.at("right"),
                         in_path + ".right",
                        &inout_rule_ptr->right_matrix_fp32vec16,
                         out_error_ptr) == false)
        {
            goto end;
        }
    }

    result = true;

end:
    return result;
}

bool Analysis::RecipeLoader::RecipeLoader::parse_baked(const Analysis::APIDumpAnalysis::JsonValue& in_baked,
                                                       const std::string&                          in_path,
                                                       Recipe*                                     inout_recipe_ptr,
                                                       std::string*                                out_error_ptr)
{
    const auto* convergence_ptr              = in_baked.find_ptr("convergence");
    const auto* covering_draws_ptr      = in_baked.find_ptr("covering_draws");
    const auto* known_signatures_ptr         = in_baked.find_ptr("known_signatures");
    bool        result                       = false;
    const auto* rule_array_ptr               = in_baked.find_ptr("draw_rules");
    const auto* scene_convergence_ptr        = in_baked.find_ptr("scene_convergence");
    const auto* scene_max_separation_ptr     = in_baked.find_ptr("scene_max_separation");
    uint32_t    signature_scheme_version     = 0;
    const auto* signature_scheme_version_ptr = in_baked.find_ptr("signature_scheme_version");

    if (in_baked.type != Analysis::APIDumpAnalysis::JsonValue::Type::OBJECT)
    {
        *out_error_ptr = in_path + ": expected an object";

        goto end;
    }

    if (check_keys(in_baked,
                   {"convergence", "covering_draws", "draw_rules", "known_signatures", "scene_convergence", "scene_max_separation", "signature_scheme_version"},
                   in_path,
                   out_error_ptr) == false)
    {
        goto end;
    }

    if (signature_scheme_version_ptr == nullptr)
    {
        *out_error_ptr = in_path + ".signature_scheme_version is missing";

        goto end;
    }

    if (parse_unsigned(*signature_scheme_version_ptr,
                       in_path + ".signature_scheme_version",
                      &signature_scheme_version,
                       out_error_ptr) == false)
    {
        goto end;
    }

    if (signature_scheme_version != Analysis::APIDumpAnalysis::PassesFormat::SIGNATURE_SCHEME_VERSION)
    {
        *out_error_ptr = in_path + ".signature_scheme_version: the signatures of version " + std::to_string(signature_scheme_version) +
                         " are not the signatures of this build (version " +
                         std::to_string(Analysis::APIDumpAnalysis::PassesFormat::SIGNATURE_SCHEME_VERSION) + "); bake the recipe again";

        goto end;
    }

    if (rule_array_ptr == nullptr)
    {
        *out_error_ptr = in_path + ".draw_rules is missing";

        goto end;
    }

    if (rule_array_ptr->type != Analysis::APIDumpAnalysis::JsonValue::Type::ARRAY)
    {
        *out_error_ptr = in_path + ".draw_rules: expected an array";

        goto end;
    }

    for (size_t n_rule = 0;
                n_rule < rule_array_ptr->item_vec.size();
              ++n_rule)
    {
        const std::string rule_path = in_path + ".draw_rules[" + std::to_string(n_rule) + "]";
        DrawRule          rule;

        if (parse_draw_rule(rule_array_ptr->item_vec.at(n_rule),
                            rule_path,
                           &rule,
                            out_error_ptr) == false)
        {
            goto end;
        }

        /* A rule which has no condition applies whatever follows it, so what follows would never be tried. */
        for (const auto& other_rule : inout_recipe_ptr->draw_rule_vec)
        {
            if (other_rule.signature_key == rule.signature_key &&
                other_rule.view_matrix_vec.empty()             )
            {
                *out_error_ptr = rule_path + ": an earlier rule for the same signature has no condition, so this one is never reached";

                goto end;
            }
        }

        inout_recipe_ptr->draw_rule_vec.push_back(rule);
    }

    if (convergence_ptr != nullptr)
    {
        KeyDistanceVector key_distance_vec;

        if (parse_key_distance_array(*convergence_ptr,
                                     in_path + ".convergence",
                                     "signature",
                                     "distance",
                                    &key_distance_vec,
                                     out_error_ptr) == false)
        {
            goto end;
        }

        for (const auto& key_distance : key_distance_vec)
        {
            DrawConvergence convergence;

            convergence.distance      = key_distance.second;
            convergence.signature_key = key_distance.first;

            inout_recipe_ptr->convergence_vec.push_back(convergence);
        }
    }

    if (scene_convergence_ptr != nullptr)
    {
        KeyDistanceVector key_distance_vec;

        if (parse_key_distance_array(*scene_convergence_ptr,
                                     in_path + ".scene_convergence",
                                     "shape",
                                     "distance",
                                    &key_distance_vec,
                                     out_error_ptr) == false)
        {
            goto end;
        }

        for (const auto& key_distance : key_distance_vec)
        {
            SceneConvergence convergence;

            convergence.distance  = key_distance.second;
            convergence.shape_key = key_distance.first;

            inout_recipe_ptr->scene_convergence_vec.push_back(convergence);
        }
    }

    if (scene_max_separation_ptr != nullptr)
    {
        KeyDistanceVector key_distance_vec;

        if (parse_key_distance_array(*scene_max_separation_ptr,
                                     in_path + ".scene_max_separation",
                                     "shape",
                                     "separation",
                                    &key_distance_vec,
                                     out_error_ptr) == false)
        {
            goto end;
        }

        for (const auto& key_distance : key_distance_vec)
        {
            SceneSeparation separation;

            separation.max_separation = key_distance.second;
            separation.shape_key      = key_distance.first;

            inout_recipe_ptr->scene_separation_vec.push_back(separation);
        }
    }

    if (covering_draws_ptr != nullptr)
    {
        if (parse_hex_key_array(*covering_draws_ptr,
                                in_path + ".covering_draws",
                               &inout_recipe_ptr->covering_draw_key_vec,
                                out_error_ptr) == false)
        {
            goto end;
        }
    }

    if (known_signatures_ptr != nullptr)
    {
        if (parse_hex_key_array(*known_signatures_ptr,
                                in_path + ".known_signatures",
                               &inout_recipe_ptr->known_signature_key_vec,
                                out_error_ptr) == false)
        {
            goto end;
        }
    }

    inout_recipe_ptr->is_baked = true;
    result                     = true;

end:
    return result;
}

bool Analysis::RecipeLoader::RecipeLoader::parse_comfort(const Analysis::APIDumpAnalysis::JsonValue& in_value,
                                                         const std::string&                          in_path,
                                                         Comfort*                                    out_comfort_ptr,
                                                         std::string*                                out_error_ptr)
{
    bool result = false;

    if (in_value.type != Analysis::APIDumpAnalysis::JsonValue::Type::OBJECT)
    {
        *out_error_ptr = in_path + ": expected an object";

        goto end;
    }

    if (check_keys(in_value,
                   {"max_near_parallax", "near_fraction", "overlay_parallax"},
                   in_path,
                   out_error_ptr) == false)
    {
        goto end;
    }

    {
        const auto* parallax_ptr = in_value.find_ptr("max_near_parallax");

        if (parallax_ptr != nullptr)
        {
            const bool is_finite = (::isfinite(parallax_ptr->number_value) != 0);

            if (parallax_ptr->type         != Analysis::APIDumpAnalysis::JsonValue::Type::NUMBER ||
                is_finite                  == false                                              ||
                parallax_ptr->number_value <= 0.0                                                ||
                parallax_ptr->number_value >  1.0)
            {
                *out_error_ptr = in_path + ".max_near_parallax: expected a number above zero, and up to one (a part of the width of the picture)";

                goto end;
            }

            out_comfort_ptr->max_near_parallax = static_cast<float>(parallax_ptr->number_value);
        }

        const auto* fraction_ptr = in_value.find_ptr("near_fraction");

        if (fraction_ptr != nullptr)
        {
            const bool is_finite = (::isfinite(fraction_ptr->number_value) != 0);

            if (fraction_ptr->type         != Analysis::APIDumpAnalysis::JsonValue::Type::NUMBER ||
                is_finite                  == false                                              ||
                fraction_ptr->number_value <= 0.0                                                ||
                fraction_ptr->number_value >= 1.0)
            {
                *out_error_ptr = in_path + ".near_fraction: expected a number above zero and below one";

                goto end;
            }

            out_comfort_ptr->near_fraction = static_cast<float>(fraction_ptr->number_value);
        }

        const auto* overlay_ptr = in_value.find_ptr("overlay_parallax");

        if (overlay_ptr != nullptr)
        {
            const bool is_finite = (::isfinite(overlay_ptr->number_value) != 0);

            if (overlay_ptr->type         != Analysis::APIDumpAnalysis::JsonValue::Type::NUMBER ||
                is_finite                 == false                                              ||
                overlay_ptr->number_value <  0.0                                                ||
                overlay_ptr->number_value >  0.5)
            {
                *out_error_ptr = in_path + ".overlay_parallax: expected a number from zero (not pushed) to a half (a part of the width of the picture)";

                goto end;
            }

            out_comfort_ptr->overlay_parallax = static_cast<float>(overlay_ptr->number_value);
        }
    }

    result = true;

end:
    return result;
}

bool Analysis::RecipeLoader::RecipeLoader::parse_draw_rule(const Analysis::APIDumpAnalysis::JsonValue& in_rule,
                                                           const std::string&                          in_path,
                                                           DrawRule*                                   out_rule_ptr,
                                                           std::string*                                out_error_ptr)
{
    const auto* class_ptr     = in_rule.find_ptr("class");
    bool        result        = false;
    const auto* signature_ptr = in_rule.find_ptr("signature");
    const auto* when_ptr      = in_rule.find_ptr("when");

    *out_rule_ptr = DrawRule();

    if (in_rule.type != Analysis::APIDumpAnalysis::JsonValue::Type::OBJECT)
    {
        *out_error_ptr = in_path + ": expected an object";

        goto end;
    }

    if (check_keys(in_rule,
                   {"class", "signature", "when"},
                   in_path,
                   out_error_ptr) == false)
    {
        goto end;
    }

    if (signature_ptr == nullptr)
    {
        *out_error_ptr = in_path + ".signature is missing";

        goto end;
    }

    if (parse_hex_key(*signature_ptr,
                      in_path + ".signature",
                     &out_rule_ptr->signature_key,
                      out_error_ptr) == false)
    {
        goto end;
    }

    if (class_ptr == nullptr)
    {
        *out_error_ptr = in_path + ".class is missing";

        goto end;
    }

    if (class_ptr->type         == Analysis::APIDumpAnalysis::JsonValue::Type::STRING &&
        class_ptr->string_value == "affected")
    {
        out_rule_ptr->draw_class = DrawClass::AFFECTED;
    }
    else
    if (class_ptr->type         == Analysis::APIDumpAnalysis::JsonValue::Type::STRING &&
        class_ptr->string_value == "skipped")
    {
        out_rule_ptr->draw_class = DrawClass::SKIPPED;
    }
    else
    if (class_ptr->type         == Analysis::APIDumpAnalysis::JsonValue::Type::STRING &&
        class_ptr->string_value == "unaffected")
    {
        out_rule_ptr->draw_class = DrawClass::UNAFFECTED;
    }
    else
    {
        *out_error_ptr = in_path + ".class: expected \"affected\", \"skipped\" or \"unaffected\"";

        goto end;
    }

    if (when_ptr != nullptr)
    {
        const auto*       constant_ptr   = when_ptr->find_ptr("constant");
        const auto*       equals_any_ptr = when_ptr->find_ptr("equals_any");
        const std::string when_path      = in_path + ".when";

        if (when_ptr->type != Analysis::APIDumpAnalysis::JsonValue::Type::OBJECT)
        {
            *out_error_ptr = when_path + ": expected an object";

            goto end;
        }

        if (check_keys(*when_ptr,
                       {"constant", "equals_any"},
                       when_path,
                       out_error_ptr) == false)
        {
            goto end;
        }

        if (constant_ptr               == nullptr                                            ||
            constant_ptr->type         != Analysis::APIDumpAnalysis::JsonValue::Type::STRING ||
            constant_ptr->string_value != "view_matrix")
        {
            *out_error_ptr = when_path + ".constant: expected \"view_matrix\", the only constant a rule can be about";

            goto end;
        }

        if (equals_any_ptr       == nullptr                                ||
            equals_any_ptr->type != Analysis::APIDumpAnalysis::JsonValue::Type::ARRAY ||
            equals_any_ptr->item_vec.empty() )
        {
            *out_error_ptr = when_path + ".equals_any: expected a list of matrices, one at least";

            goto end;
        }

        for (size_t n_matrix = 0;
                    n_matrix < equals_any_ptr->item_vec.size();
                  ++n_matrix)
        {
            std::array<float, 16> matrix = {};

            if (parse_matrix(equals_any_ptr->item_vec.at(n_matrix),
                             when_path + ".equals_any[" + std::to_string(n_matrix) + "]",
                            &matrix,
                             out_error_ptr) == false)
            {
                goto end;
            }

            out_rule_ptr->view_matrix_vec.push_back(matrix);
        }
    }

    result = true;

end:
    return result;
}

bool Analysis::RecipeLoader::RecipeLoader::parse_draw_selector(const Analysis::APIDumpAnalysis::JsonValue& in_value,
                                                               const std::string&                          in_path,
                                                               DrawSelector*                               out_selector_ptr,
                                                               std::string*                                out_error_ptr)
{
    bool result = false;

    *out_selector_ptr = DrawSelector();

    if (in_value.type != Analysis::APIDumpAnalysis::JsonValue::Type::OBJECT)
    {
        *out_error_ptr = in_path + ": expected an object with draw_keys and / or pass_keys";

        goto end;
    }

    if (check_keys(in_value,
                   {"draw_keys", "pass_keys"},
                   in_path,
                   out_error_ptr) == false)
    {
        goto end;
    }

    for (const auto& list : {std::make_pair(std::string("draw_keys"), &out_selector_ptr->draw_key_vec),
                             std::make_pair(std::string("pass_keys"), &out_selector_ptr->pass_key_vec)})
    {
        if (in_value.find_ptr(list.first) != nullptr)
        {
            if (parse_hex_key_array(in_value.at(list.first),
                                    in_path + "." + list.first,
                                    list.second,
                                    out_error_ptr) == false)
            {
                goto end;
            }

            if (list.second->empty() )
            {
                *out_error_ptr = in_path + "." + list.first + ": the list is empty; leave it out";

                goto end;
            }
        }
    }

    if (out_selector_ptr->draw_key_vec.empty() &&
        out_selector_ptr->pass_key_vec.empty() )
    {
        *out_error_ptr = in_path + ": names no draw and no pass";

        goto end;
    }

    result = true;

end:
    return result;
}

bool Analysis::RecipeLoader::RecipeLoader::parse_hex_key(const Analysis::APIDumpAnalysis::JsonValue& in_value,
                                                         const std::string&                          in_path,
                                                         uint64_t*                                   out_key_ptr,
                                                         std::string*                                out_error_ptr)
{
    constexpr int    HEX_BASE     = 16;
    constexpr size_t N_HEX_DIGITS = 16;

    bool result = false;

    *out_key_ptr = 0;

    if (in_value.type != Analysis::APIDumpAnalysis::JsonValue::Type::STRING)
    {
        *out_error_ptr = in_path + ": expected a key such as \"0x0123456789abcdef\"";

        goto end;
    }

    {
        const std::string& text = in_value.string_value;

        if (text.size()           != 2 + N_HEX_DIGITS ||
            text.compare(0, 2, "0x") != 0)
        {
            *out_error_ptr = in_path + ": expected a key such as \"0x0123456789abcdef\", not \"" + text + "\"";

            goto end;
        }

        for (size_t n_digit = 2;
                    n_digit < text.size();
                  ++n_digit)
        {
            if (::isxdigit(static_cast<unsigned char>(text.at(n_digit) )) == 0)
            {
                *out_error_ptr = in_path + ": expected a key such as \"0x0123456789abcdef\", not \"" + text + "\"";

                goto end;
            }
        }

        *out_key_ptr = ::strtoull(text.c_str() + 2,
                                  nullptr,
                                  HEX_BASE);
    }

    result = true;

end:
    return result;
}

bool Analysis::RecipeLoader::RecipeLoader::parse_hex_key_array(const Analysis::APIDumpAnalysis::JsonValue& in_array,
                                                               const std::string&                          in_path,
                                                               std::vector<uint64_t>*                      out_key_vec_ptr,
                                                               std::string*                                out_error_ptr)
{
    bool result = false;

    out_key_vec_ptr->clear();

    if (in_array.type != Analysis::APIDumpAnalysis::JsonValue::Type::ARRAY)
    {
        *out_error_ptr = in_path + ": expected an array of keys";

        goto end;
    }

    for (size_t n_key = 0;
                n_key < in_array.item_vec.size();
              ++n_key)
    {
        uint64_t key = 0;

        if (parse_hex_key(in_array.item_vec.at(n_key),
                          in_path + "[" + std::to_string(n_key) + "]",
                         &key,
                          out_error_ptr) == false)
        {
            goto end;
        }

        out_key_vec_ptr->push_back(key);
    }

    result = true;

end:
    return result;
}

bool Analysis::RecipeLoader::RecipeLoader::parse_key_distance_array(const Analysis::APIDumpAnalysis::JsonValue& in_array,
                                                                    const std::string&                          in_path,
                                                                    const std::string&                          in_key_name,
                                                                    const std::string&                          in_distance_name,
                                                                    KeyDistanceVector*                          out_key_distance_vec_ptr,
                                                                    std::string*                                out_error_ptr)
{
    bool result = false;

    if (in_array.type != Analysis::APIDumpAnalysis::JsonValue::Type::ARRAY)
    {
        *out_error_ptr = in_path + ": expected an array";

        goto end;
    }

    for (size_t n_entry = 0;
                n_entry < in_array.item_vec.size();
              ++n_entry)
    {
        const auto&       entry         = in_array.item_vec.at(n_entry);
        const std::string entry_path    = in_path + "[" + std::to_string(n_entry) + "]";
        const auto*       distance_ptr  = entry.find_ptr(in_distance_name);
        const auto*       key_ptr       = entry.find_ptr(in_key_name);
        uint64_t          key           = 0;

        if (entry.type != Analysis::APIDumpAnalysis::JsonValue::Type::OBJECT)
        {
            *out_error_ptr = entry_path + ": expected an object";

            goto end;
        }

        if (check_keys(entry,
                       {in_distance_name, in_key_name},
                       entry_path,
                       out_error_ptr) == false)
        {
            goto end;
        }

        if (key_ptr == nullptr)
        {
            *out_error_ptr = entry_path + "." + in_key_name + " is missing";

            goto end;
        }

        if (parse_hex_key(*key_ptr,
                          entry_path + "." + in_key_name,
                         &key,
                          out_error_ptr) == false)
        {
            goto end;
        }

        if (distance_ptr == nullptr)
        {
            *out_error_ptr = entry_path + "." + in_distance_name + " is missing";

            goto end;
        }

        const bool is_finite = (::isfinite(distance_ptr->number_value) != 0);

        if (distance_ptr->type         != Analysis::APIDumpAnalysis::JsonValue::Type::NUMBER ||
            is_finite                  == false                                              ||
            distance_ptr->number_value <= 0.0                                                ||
            distance_ptr->number_value >  static_cast<double>(FLT_MAX) )
        {
            *out_error_ptr = entry_path + "." + in_distance_name + ": expected a number above zero that fits a float";

            goto end;
        }

        for (const auto& other : *out_key_distance_vec_ptr)
        {
            if (other.first == key)
            {
                *out_error_ptr = entry_path + "." + in_key_name + ": the " + in_key_name + " has a " + in_distance_name + " already";

                goto end;
            }
        }

        out_key_distance_vec_ptr->push_back(std::make_pair(key,
                                                           static_cast<float>(distance_ptr->number_value) ));
    }

    result = true;

end:
    return result;
}

bool Analysis::RecipeLoader::RecipeLoader::parse_matrix(const Analysis::APIDumpAnalysis::JsonValue& in_array,
                                                        const std::string&                          in_path,
                                                        std::array<float, 16>*                      out_matrix_ptr,
                                                        std::string*                                out_error_ptr)
{
    bool result = false;

    if (in_array.type          != Analysis::APIDumpAnalysis::JsonValue::Type::ARRAY ||
        in_array.item_vec.size() != out_matrix_ptr->size() )
    {
        *out_error_ptr = in_path + ": expected 16 numbers";

        goto end;
    }

    for (size_t n_element = 0;
                n_element < out_matrix_ptr->size();
              ++n_element)
    {
        const auto& element        = in_array.item_vec.at(n_element);
        const double absolute_value = ::fabs(element.number_value);

        if (element.type    != Analysis::APIDumpAnalysis::JsonValue::Type::NUMBER ||
            absolute_value  >  static_cast<double>(FLT_MAX) )
        {
            *out_error_ptr = in_path + "[" + std::to_string(n_element) + "]: expected a number that fits a float";

            goto end;
        }

        out_matrix_ptr->at(n_element) = static_cast<float>(element.number_value);
    }

    result = true;

end:
    return result;
}

bool Analysis::RecipeLoader::RecipeLoader::parse_rule(const Analysis::APIDumpAnalysis::JsonValue& in_rule,
                                                      const std::string&                          in_path,
                                                      Rule*                                       out_rule_ptr,
                                                      std::string*                                out_error_ptr)
{
    const CallDescriptor* call_ptr = nullptr;
    bool                  result   = false;

    *out_rule_ptr = Rule();

    if (in_rule.type != Analysis::APIDumpAnalysis::JsonValue::Type::OBJECT)
    {
        *out_error_ptr = in_path + ": expected an object";

        goto end;
    }

    if (check_keys(in_rule,
                   {"action", "call", "name", "when"},
                   in_path,
                   out_error_ptr) == false)
    {
        goto end;
    }

    if (in_rule.at("name").type != Analysis::APIDumpAnalysis::JsonValue::Type::NULL_VALUE)
    {
        if (in_rule.at("name").type != Analysis::APIDumpAnalysis::JsonValue::Type::STRING)
        {
            *out_error_ptr = in_path + ".name: expected a string";

            goto end;
        }

        out_rule_ptr->name = in_rule.at("name").string_value;
    }

    if (in_rule.at("call").type != Analysis::APIDumpAnalysis::JsonValue::Type::STRING)
    {
        *out_error_ptr = in_path + ".call: expected the name of a function";

        goto end;
    }

    call_ptr = find_call_descriptor_ptr(in_rule.at("call").string_value);

    if (call_ptr == nullptr)
    {
        *out_error_ptr = in_path + ".call: \"" + in_rule.at("call").string_value + "\" is not a function that a rule can be about";

        goto end;
    }

    out_rule_ptr->function              = call_ptr->function;
    out_rule_ptr->matrix_argument_index = call_ptr->matrix_argument_index;

    if (in_rule.find_ptr("when") != nullptr)
    {
        if (parse_when(in_rule.at("when"),
                       in_path + ".when",
                      *call_ptr,
                       out_rule_ptr,
                       out_error_ptr) == false)
        {
            goto end;
        }
    }

    if (in_rule.find_ptr("action") == nullptr)
    {
        *out_error_ptr = in_path + ".action is missing";

        goto end;
    }

    if (parse_action(in_rule.at("action"),
                     in_path + ".action",
                     out_rule_ptr,
                     out_error_ptr) == false)
    {
        goto end;
    }

    result = true;

end:
    return result;
}

bool Analysis::RecipeLoader::RecipeLoader::parse_state(const Analysis::APIDumpAnalysis::JsonValue& in_value,
                                                       const std::string&                          in_path,
                                                       uint32_t*                                   out_state_ptr,
                                                       std::string*                                out_error_ptr)
{
    struct StateName
    {
        const char* name;
        uint32_t    value;
    };

    static const StateName STATE_NAME_ARRAY[] =
    {
        {"D3DTS_PROJECTION", Interception::Hooks::D3D8::D3DTS_PROJECTION},
        {"D3DTS_TEXTURE0",   Interception::Hooks::D3D8::D3DTS_TEXTURE0},
        {"D3DTS_TEXTURE1",   Interception::Hooks::D3D8::D3DTS_TEXTURE1},
        {"D3DTS_TEXTURE2",   Interception::Hooks::D3D8::D3DTS_TEXTURE2},
        {"D3DTS_TEXTURE3",   Interception::Hooks::D3D8::D3DTS_TEXTURE3},
        {"D3DTS_TEXTURE4",   Interception::Hooks::D3D8::D3DTS_TEXTURE4},
        {"D3DTS_TEXTURE5",   Interception::Hooks::D3D8::D3DTS_TEXTURE5},
        {"D3DTS_TEXTURE6",   Interception::Hooks::D3D8::D3DTS_TEXTURE6},
        {"D3DTS_TEXTURE7",   Interception::Hooks::D3D8::D3DTS_TEXTURE7},
        {"D3DTS_VIEW",       Interception::Hooks::D3D8::D3DTS_VIEW},
        {"D3DTS_WORLD",      Interception::Hooks::D3D8::D3DTS_WORLD},
        {"D3DTS_WORLD1",     Interception::Hooks::D3D8::D3DTS_WORLD1},
        {"D3DTS_WORLD2",     Interception::Hooks::D3D8::D3DTS_WORLD2},
        {"D3DTS_WORLD3",     Interception::Hooks::D3D8::D3DTS_WORLD3}
    };

    bool result = false;

    if (in_value.type == Analysis::APIDumpAnalysis::JsonValue::Type::NUMBER)
    {
        result = parse_unsigned(in_value,
                                in_path,
                                out_state_ptr,
                                out_error_ptr);
    }
    else
    if (in_value.type == Analysis::APIDumpAnalysis::JsonValue::Type::STRING)
    {
        for (const auto& state_name : STATE_NAME_ARRAY)
        {
            if (in_value.string_value == state_name.name)
            {
                *out_state_ptr = state_name.value;

                result = true;
            }
        }

        if (result == false)
        {
            *out_error_ptr = in_path + ": \"" + in_value.string_value + "\" is not a transform state";
        }
    }
    else
    {
        *out_error_ptr = in_path + ": expected a transform state such as \"D3DTS_VIEW\", or a number";
    }

    return result;
}

bool Analysis::RecipeLoader::RecipeLoader::parse_unsigned(const Analysis::APIDumpAnalysis::JsonValue& in_value,
                                                          const std::string&                          in_path,
                                                          uint32_t*                                   out_value_ptr,
                                                          std::string*                                out_error_ptr)
{
    const double floor_value = ::floor(in_value.number_value);
    bool         result      = false;

    if (in_value.type         != Analysis::APIDumpAnalysis::JsonValue::Type::NUMBER ||
        in_value.number_value <  0.0                                                ||
        in_value.number_value >  static_cast<double>(UINT32_MAX)                    ||
        in_value.number_value != floor_value)
    {
        *out_error_ptr = in_path + ": expected a whole number";
    }
    else
    {
        *out_value_ptr = static_cast<uint32_t>(in_value.number_value);

        result = true;
    }

    return result;
}

bool Analysis::RecipeLoader::RecipeLoader::parse_when(const Analysis::APIDumpAnalysis::JsonValue& in_when,
                                                      const std::string&                          in_path,
                                                      const CallDescriptor&                       in_call,
                                                      Rule*                                       inout_rule_ptr,
                                                      std::string*                                out_error_ptr)
{
    bool result = false;

    if (in_when.type != Analysis::APIDumpAnalysis::JsonValue::Type::OBJECT)
    {
        *out_error_ptr = in_path + ": expected an object";

        goto end;
    }

    if (check_keys(in_when,
                   {"matrix", "pass_keys", "shape_keys", "state"},
                   in_path,
                   out_error_ptr) == false)
    {
        goto end;
    }

    if (in_when.find_ptr("state") != nullptr)
    {
        ArgumentCondition condition;

        condition.argument_index = in_call.state_argument_index;

        if (parse_state(in_when.at("state"),
                        in_path + ".state",
                       &condition.value,
                        out_error_ptr) == false)
        {
            goto end;
        }

        inout_rule_ptr->argument_condition_vec.push_back(condition);
    }

    if (in_when.find_ptr("matrix") != nullptr)
    {
        const auto& matrix = in_when.at("matrix");

        if (matrix.type         != Analysis::APIDumpAnalysis::JsonValue::Type::STRING ||
            (matrix.string_value != "any"                                             &&
             matrix.string_value != "orthographic"                                    &&
             matrix.string_value != "perspective"))
        {
            *out_error_ptr = in_path + ".matrix: expected \"any\", \"orthographic\" or \"perspective\"";

            goto end;
        }

        if (matrix.string_value == "orthographic")
        {
            inout_rule_ptr->matrix_kind = MatrixKind::ORTHOGRAPHIC;
        }
        else
        if (matrix.string_value == "perspective")
        {
            inout_rule_ptr->matrix_kind = MatrixKind::PERSPECTIVE;
        }
    }

    if (in_when.find_ptr("pass_keys") != nullptr)
    {
        if (parse_hex_key_array(in_when.at("pass_keys"),
                                in_path + ".pass_keys",
                               &inout_rule_ptr->pass_key_vec,
                                out_error_ptr) == false)
        {
            goto end;
        }
    }

    if (in_when.find_ptr("shape_keys") != nullptr)
    {
        if (parse_hex_key_array(in_when.at("shape_keys"),
                                in_path + ".shape_keys",
                               &inout_rule_ptr->shape_key_vec,
                                out_error_ptr) == false)
        {
            goto end;
        }
    }

    result = true;

end:
    return result;
}
