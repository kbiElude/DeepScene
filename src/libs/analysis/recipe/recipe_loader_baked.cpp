/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_analysis/passes_format.h"
#include "analysis/recipe/recipe_loader.h"
#include <float.h>
#include <math.h>
#include <stdint.h>
#include <utility>

bool Analysis::Recipe::RecipeLoader::parse_baked(const Utils::Json::JsonValue& in_baked,
                                                 const std::string&            in_path,
                                                 Recipe*                       inout_recipe_ptr,
                                                 std::string*                  out_error_ptr)
{
    const auto* convergence_ptr              = in_baked.find_ptr("convergence");
    const auto* covering_draws_ptr           = in_baked.find_ptr("covering_draws");
    const auto* known_signatures_ptr         = in_baked.find_ptr("known_signatures");
    const auto* per_eye_textures_ptr         = in_baked.find_ptr("per_eye_textures");
    bool        result                       = false;
    const auto* rule_array_ptr               = in_baked.find_ptr("draw_rules");
    const auto* scene_convergence_ptr        = in_baked.find_ptr("scene_convergence");
    const auto* scene_max_separation_ptr     = in_baked.find_ptr("scene_max_separation");
    uint32_t    signature_scheme_version     = 0;
    const auto* signature_scheme_version_ptr = in_baked.find_ptr("signature_scheme_version");

    if (in_baked.type != Utils::Json::JsonValueType::OBJECT)
    {
        *out_error_ptr = in_path + ": expected an object";

        goto end;
    }

    if (check_keys(in_baked,
                   {"convergence", "covering_draws", "draw_rules", "known_signatures", "per_eye_textures", "scene_convergence", "scene_max_separation", "signature_scheme_version"},
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

    if (rule_array_ptr->type != Utils::Json::JsonValueType::ARRAY)
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

    if (per_eye_textures_ptr != nullptr)
    {
        if (parse_per_eye_textures(*per_eye_textures_ptr,
                                   in_path + ".per_eye_textures",
                                  &inout_recipe_ptr->per_eye_texture_vec,
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

bool Analysis::Recipe::RecipeLoader::parse_comfort(const Utils::Json::JsonValue& in_value,
                                                   const std::string&            in_path,
                                                   Comfort*                      out_comfort_ptr,
                                                   std::string*                  out_error_ptr)
{
    bool result = false;

    if (in_value.type != Utils::Json::JsonValueType::OBJECT)
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

            if (parallax_ptr->type         != Utils::Json::JsonValueType::NUMBER ||
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

            if (fraction_ptr->type         != Utils::Json::JsonValueType::NUMBER ||
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

            if (overlay_ptr->type         != Utils::Json::JsonValueType::NUMBER ||
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

bool Analysis::Recipe::RecipeLoader::parse_screen_parallax(const Utils::Json::JsonValue& in_value,
                                                           const std::string&            in_path,
                                                           ScreenParallax*               out_screen_parallax_ptr,
                                                           std::string*                  out_error_ptr)
{
    /* An angle of view of a half circle or more is no picture. */
    constexpr double MAX_FIELD_OF_VIEW_DEGREES = 180.0;

    bool result = false;

    if (in_value.type != Utils::Json::JsonValueType::OBJECT)
    {
        *out_error_ptr = in_path + ": expected an object";

        goto end;
    }

    if (check_keys(in_value,
                   {"convergence_distance", "field_of_view_degrees"},
                   in_path,
                   out_error_ptr) == false)
    {
        goto end;
    }

    {
        const auto* distance_ptr = in_value.find_ptr("convergence_distance");
        const auto* fov_ptr      = in_value.find_ptr("field_of_view_degrees");

        /* The distance is what there is no way to find out. */
        if (distance_ptr == nullptr)
        {
            *out_error_ptr = in_path + ".convergence_distance is missing";

            goto end;
        }

        if (distance_ptr->type         != Utils::Json::JsonValueType::NUMBER ||
            ::isfinite(distance_ptr->number_value) == 0                                      ||
            distance_ptr->number_value <= 0.0)
        {
            *out_error_ptr = in_path + ".convergence_distance: expected a number above zero (the distance at which the eyes converge, in the "
                                       "units of the camera of the application)";

            goto end;
        }

        out_screen_parallax_ptr->convergence_distance = static_cast<float>(distance_ptr->number_value);

        if (fov_ptr != nullptr)
        {
            if (fov_ptr->type         != Utils::Json::JsonValueType::NUMBER ||
                ::isfinite(fov_ptr->number_value) == 0                                      ||
                fov_ptr->number_value <= 0.0                                                ||
                fov_ptr->number_value >= MAX_FIELD_OF_VIEW_DEGREES)
            {
                *out_error_ptr = in_path + ".field_of_view_degrees: expected a number above zero and below 180 (the angle of view of the width of the picture)";

                goto end;
            }

            out_screen_parallax_ptr->field_of_view_degrees = static_cast<float>(fov_ptr->number_value);
        }

        out_screen_parallax_ptr->is_enabled = true;
    }

    result = true;

end:
    return result;
}

bool Analysis::Recipe::RecipeLoader::parse_draw_rule(const Utils::Json::JsonValue& in_rule,
                                                     const std::string&            in_path,
                                                     DrawRule*                     out_rule_ptr,
                                                     std::string*                  out_error_ptr)
{
    const auto* class_ptr     = in_rule.find_ptr("class");
    bool        result        = false;
    const auto* signature_ptr = in_rule.find_ptr("signature");
    const auto* when_ptr      = in_rule.find_ptr("when");

    *out_rule_ptr = DrawRule();

    if (in_rule.type != Utils::Json::JsonValueType::OBJECT)
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

    if (class_ptr->type         == Utils::Json::JsonValueType::STRING &&
        class_ptr->string_value == "affected")
    {
        out_rule_ptr->draw_class = DrawClass::AFFECTED;
    }
    else
    if (class_ptr->type         == Utils::Json::JsonValueType::STRING &&
        class_ptr->string_value == "skipped")
    {
        out_rule_ptr->draw_class = DrawClass::SKIPPED;
    }
    else
    if (class_ptr->type         == Utils::Json::JsonValueType::STRING &&
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

        if (when_ptr->type != Utils::Json::JsonValueType::OBJECT)
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
            constant_ptr->type         != Utils::Json::JsonValueType::STRING ||
            constant_ptr->string_value != "view_matrix")
        {
            *out_error_ptr = when_path + ".constant: expected \"view_matrix\", the only constant a rule can be about";

            goto end;
        }

        if (equals_any_ptr       == nullptr                                ||
            equals_any_ptr->type != Utils::Json::JsonValueType::ARRAY ||
            equals_any_ptr->item_vec.empty() )
        {
            *out_error_ptr = when_path + ".equals_any: expected a list of matrices, one at least";

            goto end;
        }

        for (size_t n_matrix = 0;
                    n_matrix < equals_any_ptr->item_vec.size();
                  ++n_matrix)
        {
            std::array<float, 16> matrix      = {};
            const std::string     matrix_path = when_path + ".equals_any[" + std::to_string(n_matrix) + "]";

            if (parse_matrix(equals_any_ptr->item_vec.at(n_matrix),
                             matrix_path,
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

bool Analysis::Recipe::RecipeLoader::parse_draw_selector(const Utils::Json::JsonValue& in_value,
                                                         const std::string&            in_path,
                                                         DrawSelector*                 out_selector_ptr,
                                                         std::string*                  out_error_ptr)
{
    bool result = false;

    *out_selector_ptr = DrawSelector();

    if (in_value.type != Utils::Json::JsonValueType::OBJECT)
    {
        *out_error_ptr = in_path + ": expected an object with draw_keys, pass_keys and / or pass_draws";

        goto end;
    }

    if (check_keys(in_value,
                   {"draw_keys", "pass_draws", "pass_keys"},
                   in_path,
                   out_error_ptr) == false)
    {
        goto end;
    }

    if (in_value.find_ptr("pass_draws") != nullptr)
    {
        if (parse_pass_draw_array(in_value.at("pass_draws"),
                                  in_path + ".pass_draws",
                                 &out_selector_ptr->pass_draw_vec,
                                  out_error_ptr) == false)
        {
            goto end;
        }

        if (out_selector_ptr->pass_draw_vec.empty() )
        {
            *out_error_ptr = in_path + ".pass_draws: the list is empty; leave it out";

            goto end;
        }
    }

    {
        struct KeyList
        {
            const char*            name_ptr;
            std::vector<uint64_t>* key_vec_ptr;
        };

        const KeyList key_list_array[] =
        {
            {"draw_keys", &out_selector_ptr->draw_key_vec},
            {"pass_keys", &out_selector_ptr->pass_key_vec}
        };

        for (const auto& list : key_list_array)
        {
            const std::string list_name = list.name_ptr;

            if (in_value.find_ptr(list_name) != nullptr)
            {
                const auto&       key_array = in_value.at(list_name);
                const std::string list_path = in_path + "." + list_name;

                if (parse_hex_key_array(key_array,
                                        list_path,
                                        list.key_vec_ptr,
                                        out_error_ptr) == false)
                {
                    goto end;
                }

                if (list.key_vec_ptr->empty() )
                {
                    *out_error_ptr = list_path + ": the list is empty; leave it out";

                    goto end;
                }
            }
        }
    }

    if (out_selector_ptr->draw_key_vec.empty()  &&
        out_selector_ptr->pass_draw_vec.empty() &&
        out_selector_ptr->pass_key_vec.empty() )
    {
        *out_error_ptr = in_path + ": names no draw and no pass";

        goto end;
    }

    result = true;

end:
    return result;
}

bool Analysis::Recipe::RecipeLoader::parse_key_distance_array(const Utils::Json::JsonValue& in_array,
                                                              const std::string&            in_path,
                                                              const std::string&            in_key_name,
                                                              const std::string&            in_distance_name,
                                                              KeyDistanceVector*            out_key_distance_vec_ptr,
                                                              std::string*                  out_error_ptr)
{
    bool result = false;

    if (in_array.type != Utils::Json::JsonValueType::ARRAY)
    {
        *out_error_ptr = in_path + ": expected an array";

        goto end;
    }

    for (size_t n_entry = 0;
                n_entry < in_array.item_vec.size();
              ++n_entry)
    {
        const auto&       entry        = in_array.item_vec.at(n_entry);
        const std::string entry_path   = in_path + "[" + std::to_string(n_entry) + "]";
        const auto*       distance_ptr = entry.find_ptr(in_distance_name);
        const auto*       key_ptr      = entry.find_ptr(in_key_name);
        uint64_t          key          = 0;

        if (entry.type != Utils::Json::JsonValueType::OBJECT)
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

        if (distance_ptr->type         != Utils::Json::JsonValueType::NUMBER ||
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

bool Analysis::Recipe::RecipeLoader::parse_pass_draw_array(const Utils::Json::JsonValue& in_array,
                                                           const std::string&            in_path,
                                                           std::vector<PassDraw>*        out_pass_draw_vec_ptr,
                                                           std::string*                  out_error_ptr)
{
    bool result = false;

    out_pass_draw_vec_ptr->clear();

    if (in_array.type != Utils::Json::JsonValueType::ARRAY)
    {
        *out_error_ptr = in_path + ": expected an array of objects with a pass_key and a draw_key";

        goto end;
    }

    for (size_t n_item = 0;
                n_item < in_array.item_vec.size();
              ++n_item)
    {
        const auto&       item      = in_array.item_vec.at(n_item);
        const std::string path      = in_path + "[" + std::to_string(n_item) + "]";
        PassDraw          pass_draw;

        if (item.type != Utils::Json::JsonValueType::OBJECT)
        {
            *out_error_ptr = path + ": expected an object with a pass_key and a draw_key";

            goto end;
        }

        if (check_keys(item,
                       {"draw_key", "pass_key"},
                       path,
                       out_error_ptr) == false)
        {
            goto end;
        }

        if (item.find_ptr("draw_key") == nullptr ||
            item.find_ptr("pass_key") == nullptr)
        {
            *out_error_ptr = path + ": a draw in a pass needs both a pass_key and a draw_key";

            goto end;
        }

        if (parse_hex_key(item.at("draw_key"),
                          path + ".draw_key",
                         &pass_draw.draw_key,
                          out_error_ptr) == false)
        {
            goto end;
        }

        if (parse_hex_key(item.at("pass_key"),
                          path + ".pass_key",
                         &pass_draw.pass_key,
                          out_error_ptr) == false)
        {
            goto end;
        }

        out_pass_draw_vec_ptr->push_back(pass_draw);
    }

    result = true;

end:
    return result;
}

bool Analysis::Recipe::RecipeLoader::parse_per_eye_textures(const Utils::Json::JsonValue& in_value,
                                                            const std::string&            in_path,
                                                            std::vector<PerEyeTexture>*   out_texture_vec_ptr,
                                                            std::string*                  out_error_ptr)
{
    bool result = false;

    if (in_value.type != Utils::Json::JsonValueType::ARRAY)
    {
        *out_error_ptr = in_path + ": expected an array";

        goto end;
    }

    for (size_t n_entry = 0;
                n_entry < in_value.item_vec.size();
              ++n_entry)
    {
        const auto&       entry       = in_value.item_vec.at(n_entry);
        const std::string entry_path  = in_path + "[" + std::to_string(n_entry) + "]";
        const auto*       key_ptr     = entry.find_ptr("description_key");
        const auto*       ordinal_ptr = entry.find_ptr("ordinal");
        PerEyeTexture     texture;

        if (entry.type != Utils::Json::JsonValueType::OBJECT)
        {
            *out_error_ptr = entry_path + ": expected an object";

            goto end;
        }

        if (check_keys(entry,
                       {"description_key", "ordinal"},
                       entry_path,
                       out_error_ptr) == false)
        {
            goto end;
        }

        if (key_ptr     == nullptr ||
            ordinal_ptr == nullptr)
        {
            *out_error_ptr = entry_path + ": both the description_key and the ordinal are needed";

            goto end;
        }

        if (parse_hex_key(*key_ptr,
                          entry_path + ".description_key",
                         &texture.description_key,
                          out_error_ptr) == false)
        {
            goto end;
        }

        if (parse_unsigned(*ordinal_ptr,
                           entry_path + ".ordinal",
                          &texture.ordinal,
                           out_error_ptr) == false)
        {
            goto end;
        }

        for (const auto& other : *out_texture_vec_ptr)
        {
            if (other.description_key == texture.description_key &&
                other.ordinal         == texture.ordinal)
            {
                *out_error_ptr = entry_path + ": the texture is in the list already";

                goto end;
            }
        }

        out_texture_vec_ptr->push_back(texture);
    }

    result = true;

end:
    return result;
}
