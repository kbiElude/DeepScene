/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/recipe/recipe_loader.h"
#include "analysis/api_dump_analysis/passes_format.h"
#include "interception/core/D3D7Utils/constants.h"
#include "interception/hooks/D3D8/types.h"
#include <algorithm>
#include <ctype.h>
#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <utility>

bool Analysis::Recipe::RecipeLoader::check_keys(const Utils::Json::JsonValue&   in_object,
                                                const std::vector<std::string>& in_allowed_key_vec,
                                                const std::string&              in_path,
                                                std::string*                    out_error_ptr)
{
    const std::string* unknown_key_ptr = in_object.find_unknown_key_ptr(in_allowed_key_vec);

    if (unknown_key_ptr != nullptr)
    {
        *out_error_ptr = ((in_path.empty() ) ? std::string()
                                             : in_path + ": ") + "unknown field \"" + *unknown_key_ptr + "\"";
    }

    return (unknown_key_ptr == nullptr);
}

std::vector<std::string> Analysis::Recipe::RecipeLoader::describe_pass_selectors_live_ignores(const Recipe& in_recipe)
{
    struct Named
    {
        const DrawSelector* selector_ptr;
        const char*         name;
    };

    const Named              named_array[] =
    {
        {&in_recipe.affected,   "affected"  },
        {&in_recipe.pushed,     "pushed"    },
        {&in_recipe.skipped,    "skipped"   },
        {&in_recipe.unaffected, "unaffected"}
    };
    std::vector<std::string> result;

    for (const auto& named : named_array)
    {
        if (named.selector_ptr->pass_key_vec.empty() == false)
        {
            result.push_back(std::string("\"")                                                                                      +
                             named.name                                                                                             +
                             ".pass_keys\" names "                                                                                  +
                             std::to_string(named.selector_ptr->pass_key_vec.size() )                                               +
                             " passes: a converter that plays the application as it runs does not know passes, and leaves them out");
        }

        if (named.selector_ptr->pass_draw_vec.empty() == false)
        {
            result.push_back(std::string("\"")                                                                                                +
                             named.name                                                                                                       +
                             ".pass_draws\" names "                                                                                           +
                             std::to_string(named.selector_ptr->pass_draw_vec.size() )                                                        +
                             " draws in a pass: a converter that plays the application as it runs does not know passes, and leaves them out");
        }
    }

    return result;
}

const Analysis::Recipe::RecipeLoader::CallDescriptor* Analysis::Recipe::RecipeLoader::find_call_descriptor_ptr(const std::string& in_name)
{
    /* The functions that a rule can apply to, with their API, name and argument positions. Argument 0 is the object that the function is
     * called on.
     */
    static const CallDescriptor CALL_DESCRIPTOR_ARRAY[] =
    {
        {RuleApi::D3D7, APIInterceptor::APIFunction::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_SETTRANSFORM, 2, "IDirect3DDevice7::SetTransform", 1},
        {RuleApi::D3D8, APIInterceptor::APIFunction::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_SETTRANSFORM, 2, "IDirect3DDevice8::SetTransform", 1}
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

bool Analysis::Recipe::RecipeLoader::has_common_key(const std::vector<uint64_t>& in_a,
                                                    const std::vector<uint64_t>& in_b)
{
    bool result = false;

    for (const auto& key : in_a)
    {
        if (std::find(in_b.begin(),
                      in_b.end  (),
                      key)          != in_b.end() )
        {
            result = true;
        }
    }

    return result;
}

bool Analysis::Recipe::RecipeLoader::has_common_pass_draw(const std::vector<PassDraw>& in_a,
                                                          const std::vector<PassDraw>& in_b)
{
    bool result = false;

    for (const auto& pass_draw : in_a)
    {
        for (const auto& other : in_b)
        {
            result = (result || (pass_draw.draw_key == other.draw_key &&
                                 pass_draw.pass_key == other.pass_key) );
        }
    }

    return result;
}

bool Analysis::Recipe::RecipeLoader::load(const std::string& in_filename,
                                          Recipe*            out_recipe_ptr,
                                          std::string*       out_error_ptr)
{
    bool        result = false;
    std::string text;

    if (Utils::Json::JsonReader::read_file(in_filename,
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

bool Analysis::Recipe::RecipeLoader::parse(const std::string& in_text,
                                           Recipe*            out_recipe_ptr,
                                           std::string*       out_error_ptr)
{
    constexpr uint32_t SUPPORTED_RECIPE_VERSION = 0;

    std::string            json_error;
    Utils::Json::JsonValue root;
    bool                   result     = false;

    *out_recipe_ptr = Recipe();

    const std::string text = Utils::Json::JsonReader::strip_byte_order_mark(in_text);

    if (Utils::Json::JsonReader::parse(text,
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

    if (check_keys(root,
                   {"affected", "baked", "cache_pixel_reads", "comfort", "key_scheme_version", "known_shape_keys", "on_unknown_frame", "pushed", "recipe_version", "rules", "screen_parallax", "skipped", "target", "unaffected", "view_anchor_shift"},
                   "",
                   out_error_ptr) == false)
    {
        goto end;
    }

    {
        const auto* affected_ptr           = root.find_ptr("affected");
        const auto* baked_ptr              = root.find_ptr("baked");
        const auto* cache_pixel_reads_ptr  = root.find_ptr("cache_pixel_reads");
        const auto* comfort_ptr            = root.find_ptr("comfort");
        const auto* key_scheme_version_ptr = root.find_ptr("key_scheme_version");
        const auto* known_shape_keys_ptr   = root.find_ptr("known_shape_keys");
        const auto* on_unknown_frame_ptr   = root.find_ptr("on_unknown_frame");
        const auto* pushed_ptr             = root.find_ptr("pushed");
        const auto* recipe_version_ptr     = root.find_ptr("recipe_version");
        const auto* rule_array_ptr         = root.find_ptr("rules");
        const auto* screen_parallax_ptr    = root.find_ptr("screen_parallax");
        const auto* skipped_ptr            = root.find_ptr("skipped");
        const auto* target_ptr             = root.find_ptr("target");
        const auto* unaffected_ptr         = root.find_ptr("unaffected");
        const auto* view_anchor_shift_ptr  = root.find_ptr("view_anchor_shift");

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
            *out_error_ptr = "recipe_version: this version reads recipes of version " +
                             std::to_string(SUPPORTED_RECIPE_VERSION)                 +
                             ", not "                                                 +
                             std::to_string(out_recipe_ptr->version);

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
            *out_error_ptr = "key_scheme_version: the keys of version "                                  +
                             std::to_string(out_recipe_ptr->key_scheme_version)                          +
                             " are not the keys of this build (version "                                 +
                             std::to_string(Analysis::APIDumpAnalysis::PassesFormat::KEY_SCHEME_VERSION) +
                             ")";

            goto end;
        }

        if (target_ptr != nullptr)
        {
            if (target_ptr->type != Utils::Json::JsonValueType::OBJECT)
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

            if (target_ptr->at("description").type != Utils::Json::JsonValueType::NULL_VALUE)
            {
                if (target_ptr->at("description").type != Utils::Json::JsonValueType::STRING)
                {
                    *out_error_ptr = "target.description: expected a string";

                    goto end;
                }

                out_recipe_ptr->target_description = target_ptr->at("description").string_value;
            }

            if (target_ptr->at("name").type != Utils::Json::JsonValueType::NULL_VALUE)
            {
                if (target_ptr->at("name").type != Utils::Json::JsonValueType::STRING)
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

            /* A recipe without a known_shape_keys field means that every frame is known. An empty list would mean that no frame is known, so it
             * is refused.
             */
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

        if (pushed_ptr != nullptr)
        {
            if (parse_draw_selector(*pushed_ptr,
                                    "pushed",
                                   &out_recipe_ptr->pushed,
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

        /* Only `pushed` may name draws by cover key, because only the push uses the size of a draw. The replay and the converter that plays the
         * application while it runs both know that size.
         */
        {
            struct Named
            {
                const DrawSelector* selector_ptr;
                const char*         name_ptr;
            };

            const Named selector_array[] =
            {
                {&out_recipe_ptr->affected,   "affected"  },
                {&out_recipe_ptr->skipped,    "skipped"   },
                {&out_recipe_ptr->unaffected, "unaffected"}
            };

            for (const auto& named : selector_array)
            {
                if (named.selector_ptr->cover_key_vec.empty() == false)
                {
                    *out_error_ptr = std::string(named.name_ptr) + ".cover_keys: only \"pushed\" can name draws by their size";

                    goto end;
                }

                if (named.selector_ptr->part_cover_key_vec.empty() == false)
                {
                    *out_error_ptr = std::string(named.name_ptr) + ".part_cover_keys: only \"pushed\" can name draws by their size";

                    goto end;
                }
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

        if (screen_parallax_ptr != nullptr)
        {
            if (parse_screen_parallax(*screen_parallax_ptr,
                                      "screen_parallax",
                                     &out_recipe_ptr->screen_parallax,
                                      out_error_ptr) == false)
            {
                goto end;
            }
        }

        if (view_anchor_shift_ptr != nullptr)
        {
            if (parse_anchor_shift(*view_anchor_shift_ptr,
                                        "view_anchor_shift",
                                       &out_recipe_ptr->view_anchor_shift,
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

        const bool has_common_draw_key      = has_common_key      (out_recipe_ptr->affected.draw_key_vec,
                                                                   out_recipe_ptr->unaffected.draw_key_vec);
        const bool has_common_pass_draw_key = has_common_pass_draw(out_recipe_ptr->affected.pass_draw_vec,
                                                                   out_recipe_ptr->unaffected.pass_draw_vec);
        const bool has_common_pass_key      = has_common_key      (out_recipe_ptr->affected.pass_key_vec,
                                                                   out_recipe_ptr->unaffected.pass_key_vec);

        if (has_common_draw_key      ||
            has_common_pass_draw_key ||
            has_common_pass_key)
        {
            *out_error_ptr = "affected and unaffected: a key is in both";

            goto end;
        }


        if (cache_pixel_reads_ptr != nullptr)
        {
            if (cache_pixel_reads_ptr->type != Utils::Json::JsonValueType::BOOL)
            {
                *out_error_ptr = "cache_pixel_reads: expected true or false";

                goto end;
            }

            out_recipe_ptr->caches_pixel_reads = cache_pixel_reads_ptr->bool_value;
        }

        if (on_unknown_frame_ptr != nullptr)
        {
            if (on_unknown_frame_ptr->type != Utils::Json::JsonValueType::STRING)
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
                                 on_unknown_frame_ptr->string_value                                  +
                                 "\"";

                goto end;
            }
        }

        if (rule_array_ptr == nullptr)
        {
            *out_error_ptr = "rules is missing";

            goto end;
        }

        if (rule_array_ptr->type != Utils::Json::JsonValueType::ARRAY)
        {
            *out_error_ptr = "rules: expected an array";

            goto end;
        }

        for (size_t n_rule = 0;
                    n_rule < rule_array_ptr->item_vec.size();
                  ++n_rule)
        {
            const std::string rule_path = "rules[" + std::to_string(n_rule) + "]";
            MatrixRule        rule;

            if (parse_rule(rule_array_ptr->item_vec.at(n_rule),
                           rule_path,
                          &rule,
                           out_error_ptr) == false)
            {
                goto end;
            }

            for (const auto& other_rule : out_recipe_ptr->rule_vec)
            {
                if (rule.name.empty() == false &&
                    rule.name         == other_rule.name)
                {
                    *out_error_ptr = "rules["                    +
                                     std::to_string(n_rule)      +
                                     "].name: \""                +
                                     rule.name                   +
                                     "\" is used by another rule";

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

bool Analysis::Recipe::RecipeLoader::parse_action(const Utils::Json::JsonValue& in_action,
                                                  const std::string&            in_path,
                                                  MatrixRule*                   inout_rule_ptr,
                                                  std::string*                  out_error_ptr)
{
    bool result = false;

    if (in_action.type != Utils::Json::JsonValueType::OBJECT)
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

        if (type_ptr->type          != Utils::Json::JsonValueType::STRING ||
            (type_ptr->string_value != "eye_convergence"                  &&
             type_ptr->string_value != "eye_offset"                       &&
             type_ptr->string_value != "multiply"                         &&
             type_ptr->string_value != "replace"))
        {
            *out_error_ptr = in_path + ".type: expected \"eye_convergence\", \"eye_offset\", \"multiply\" or \"replace\"";

            goto end;
        }

        inout_rule_ptr->action_kind = (type_ptr->string_value == "eye_convergence") ? ActionKind::EYE_CONVERGENCE
                                    : (type_ptr->string_value == "eye_offset")      ? ActionKind::EYE_OFFSET
                                    : (type_ptr->string_value == "multiply")        ? ActionKind::MULTIPLY
                                                                                    : ActionKind::REPLACE;

        if (inout_rule_ptr->action_kind != ActionKind::EYE_OFFSET)
        {
            const auto* stray_axis_ptr = in_action.find_ptr("axis");

            if (stray_axis_ptr != nullptr)
            {
                *out_error_ptr = in_path + ".axis: only eye_offset has an axis";

                goto end;
            }
        }

        if (inout_rule_ptr->action_kind == ActionKind::EYE_CONVERGENCE)
        {
            const auto* left_ptr  = in_action.find_ptr("left");
            const auto* right_ptr = in_action.find_ptr("right");

            if (order_ptr != nullptr ||
                left_ptr  != nullptr ||
                right_ptr != nullptr)
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

            if (order_ptr->type          != Utils::Json::JsonValueType::STRING ||
                (order_ptr->string_value != "post"                             &&
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

            if (axis_ptr->type          != Utils::Json::JsonValueType::STRING ||
                (axis_ptr->string_value != "x"                                &&
                 axis_ptr->string_value != "y"                                &&
                 axis_ptr->string_value != "z"))
            {
                *out_error_ptr = in_path + ".axis: expected \"x\", \"y\" or \"z\"";

                goto end;
            }

            inout_rule_ptr->offset_axis = (axis_ptr->string_value == "x") ? Axis::X
                                        : (axis_ptr->string_value == "y") ? Axis::Y
                                                                          : Axis::Z;

            const auto* left_ptr  = in_action.find_ptr("left");
            const auto* right_ptr = in_action.find_ptr("right");

            if (left_ptr  != nullptr ||
                right_ptr != nullptr)
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

bool Analysis::Recipe::RecipeLoader::parse_anchor_shift(const Utils::Json::JsonValue& in_value,
                                                        const std::string&            in_path,
                                                        ViewAnchorShift*              out_shift_ptr,
                                                        std::string*                  out_error_ptr)
{
    bool result = false;

    if (in_value.type != Utils::Json::JsonValueType::OBJECT)
    {
        *out_error_ptr = in_path + ": expected an object";

        goto end;
    }

    if (check_keys(in_value,
                   {"convergence_distance", "draws_without_anchor"},
                   in_path,
                   out_error_ptr) == false)
    {
        goto end;
    }

    {
        const auto* distance_ptr = in_value.find_ptr("convergence_distance");
        const auto* draws_ptr    = in_value.find_ptr("draws_without_anchor");

        /* The distance is optional. A scene (see SceneConvergence) provides its own, and without either distance the eyes are only moved apart. */
        if (distance_ptr != nullptr)
        {
            bool is_distance_valid = false;

            if (distance_ptr->type == Utils::Json::JsonValueType::NUMBER)
            {
                const bool is_distance_finite = (::isfinite(distance_ptr->number_value) != 0);

                is_distance_valid = (is_distance_finite &&
                                     distance_ptr->number_value > 0.0);
            }

            if (is_distance_valid == false)
            {
                *out_error_ptr = in_path + ".convergence_distance: expected a number above zero (the distance at which the eyes converge, in the units of the camera "
                                           "of the application)";

                goto end;
            }

            out_shift_ptr->convergence_distance = static_cast<float>(distance_ptr->number_value);
        }

        if (draws_ptr != nullptr)
        {
            const bool is_valid = (draws_ptr->type == Utils::Json::JsonValueType::STRING                 &&
                                   (draws_ptr->string_value == "fail" || draws_ptr->string_value == "leave") );

            if (is_valid == false)
            {
                *out_error_ptr = in_path + ".draws_without_anchor: expected \"fail\" (a draw that the recipe affects and that has no camera to move stops the replay, and is told) or \"leave\" (it is played as it is)";

                goto end;
            }

            out_shift_ptr->leaves_draws_without_anchor = (draws_ptr->string_value == "leave");
        }

        out_shift_ptr->is_enabled = true;
    }

    result = true;

end:
    return result;
}

bool Analysis::Recipe::RecipeLoader::parse_baked(const Utils::Json::JsonValue& in_baked,
                                                 const std::string&            in_path,
                                                 Recipe*                       inout_recipe_ptr,
                                                 std::string*                  out_error_ptr)
{
    const auto* convergence_ptr              = in_baked.find_ptr("convergence");
    const auto* covering_draws_ptr           = in_baked.find_ptr("covering_draws");
    const auto* eye_space_draws_ptr          = in_baked.find_ptr("eye_space_draws");
    const auto* flat_layer_draws_ptr         = in_baked.find_ptr("flat_layer_draws");
    const auto* known_signatures_ptr         = in_baked.find_ptr("known_signatures");
    const auto* overlay_draws_ptr            = in_baked.find_ptr("overlay_draws");
    const auto* per_eye_textures_ptr         = in_baked.find_ptr("per_eye_textures");
    bool        result                       = false;
    const auto* rule_array_ptr               = in_baked.find_ptr("draw_rules");
    const auto* scene_convergence_ptr        = in_baked.find_ptr("scene_convergence");
    const auto* scene_max_separation_ptr     = in_baked.find_ptr("scene_max_separation");
    const auto* side_reaching_draws_ptr      = in_baked.find_ptr("side_reaching_draws");
    uint32_t    signature_scheme_version     = 0;
    const auto* signature_scheme_version_ptr = in_baked.find_ptr("signature_scheme_version");

    if (in_baked.type != Utils::Json::JsonValueType::OBJECT)
    {
        *out_error_ptr = in_path + ": expected an object";

        goto end;
    }

    if (check_keys(in_baked,
                   {"convergence", "covering_draws", "draw_rules", "eye_space_draws", "flat_layer_draws", "known_signatures", "overlay_draws", "per_eye_textures", "scene_convergence", "scene_max_separation", "side_reaching_draws", "signature_scheme_version"},
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
        *out_error_ptr = in_path                                                                           +
                         ".signature_scheme_version: the signatures of version "                           +
                         std::to_string(signature_scheme_version)                                          +
                         " are not the signatures of this build (version "                                 +
                         std::to_string(Analysis::APIDumpAnalysis::PassesFormat::SIGNATURE_SCHEME_VERSION) +
                         "); bake the recipe again";

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

        /* An earlier rule for the same signature that has no condition always applies, so this rule would never be tried. */
        for (const auto& other_rule : inout_recipe_ptr->draw_rule_vec)
        {
            if (other_rule.signature_key           == rule.signature_key &&
                other_rule.view_matrix_vec.empty()                       )
            {
                *out_error_ptr = rule_path                                                                                 +
                                 ": an earlier rule for the same signature has no condition, so this one is never reached";

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

    if (eye_space_draws_ptr != nullptr)
    {
        if (parse_hex_key_array(*eye_space_draws_ptr,
                                in_path + ".eye_space_draws",
                               &inout_recipe_ptr->eye_space_draw_key_vec,
                                out_error_ptr) == false)
        {
            goto end;
        }
    }

    if (flat_layer_draws_ptr != nullptr)
    {
        if (parse_hex_key_array(*flat_layer_draws_ptr,
                                in_path + ".flat_layer_draws",
                               &inout_recipe_ptr->flat_layer_draw_key_vec,
                                out_error_ptr) == false)
        {
            goto end;
        }
    }

    if (overlay_draws_ptr != nullptr)
    {
        if (parse_hex_key_array(*overlay_draws_ptr,
                                in_path + ".overlay_draws",
                               &inout_recipe_ptr->overlay_draw_key_vec,
                                out_error_ptr) == false)
        {
            goto end;
        }
    }

    if (side_reaching_draws_ptr != nullptr)
    {
        if (parse_hex_key_array(*side_reaching_draws_ptr,
                                in_path + ".side_reaching_draws",
                               &inout_recipe_ptr->side_reaching_draw_key_vec,
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
                   {"automatic_layers", "balance_convergence", "camera_attached_depth", "ease_by_time", "eye_space_cover_keys", "max_far_parallax", "max_near_parallax", "measure_depth", "near_fraction", "overlay_parallax", "part_parallax", "scene_overrides", "snap_at_cuts"},
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
                is_finite                  == false                              ||
                parallax_ptr->number_value <= 0.0                                ||
                parallax_ptr->number_value >  1.0)
            {
                *out_error_ptr = in_path + ".max_near_parallax: expected a number above zero, and up to one (a part of the width of the picture)";

                goto end;
            }

            out_comfort_ptr->max_near_parallax = static_cast<float>(parallax_ptr->number_value);
        }

        const auto* automatic_ptr = in_value.find_ptr("automatic_layers");

        if (automatic_ptr != nullptr)
        {
            if (automatic_ptr->type != Utils::Json::JsonValueType::BOOL)
            {
                *out_error_ptr = in_path + ".automatic_layers: expected true or false";

                goto end;
            }

            out_comfort_ptr->uses_automatic_layers = automatic_ptr->bool_value;
        }

        const auto* balance_ptr = in_value.find_ptr("balance_convergence");

        if (balance_ptr != nullptr)
        {
            if (balance_ptr->type != Utils::Json::JsonValueType::BOOL)
            {
                *out_error_ptr = in_path + ".balance_convergence: expected true or false";

                goto end;
            }

            out_comfort_ptr->balances_convergence = balance_ptr->bool_value;
        }

        const auto* ease_ptr = in_value.find_ptr("ease_by_time");

        if (ease_ptr != nullptr)
        {
            if (ease_ptr->type != Utils::Json::JsonValueType::BOOL)
            {
                *out_error_ptr = in_path + ".ease_by_time: expected true or false";

                goto end;
            }

            out_comfort_ptr->eases_by_time = ease_ptr->bool_value;
        }

        const auto* snap_ptr = in_value.find_ptr("snap_at_cuts");

        if (snap_ptr != nullptr)
        {
            if (snap_ptr->type != Utils::Json::JsonValueType::BOOL)
            {
                *out_error_ptr = in_path + ".snap_at_cuts: expected true or false";

                goto end;
            }

            out_comfort_ptr->snaps_at_cuts = snap_ptr->bool_value;
        }

        const auto* attached_ptr = in_value.find_ptr("camera_attached_depth");

        if (attached_ptr != nullptr)
        {
            const bool is_finite = (::isfinite(attached_ptr->number_value) != 0);

            if (attached_ptr->type         != Utils::Json::JsonValueType::NUMBER ||
                is_finite                  == false                              ||
                attached_ptr->number_value <= 0.0                                ||
                attached_ptr->number_value >= 1.0)
            {
                *out_error_ptr = in_path + ".camera_attached_depth: expected a number above zero and below one (a part of the distance at which the eyes converge)";

                goto end;
            }

            out_comfort_ptr->camera_attached_depth = static_cast<float>(attached_ptr->number_value);
        }

        const auto* eye_space_ptr = in_value.find_ptr("eye_space_cover_keys");

        if (eye_space_ptr != nullptr)
        {
            if (parse_hex_key_array(*eye_space_ptr,
                                    in_path + ".eye_space_cover_keys",
                                   &out_comfort_ptr->eye_space_cover_key_vec,
                                    out_error_ptr) == false)
            {
                goto end;
            }
        }

        const auto* measure_ptr = in_value.find_ptr("measure_depth");

        if (measure_ptr != nullptr)
        {
            if (measure_ptr->type != Utils::Json::JsonValueType::BOOL)
            {
                *out_error_ptr = in_path + ".measure_depth: expected true or false";

                goto end;
            }

            out_comfort_ptr->measures_depth = measure_ptr->bool_value;
        }

        const auto* far_ptr = in_value.find_ptr("max_far_parallax");

        if (far_ptr != nullptr)
        {
            const bool is_finite = (::isfinite(far_ptr->number_value) != 0);

            if (far_ptr->type         != Utils::Json::JsonValueType::NUMBER ||
                is_finite             == false                              ||
                far_ptr->number_value <= 0.0                                ||
                far_ptr->number_value >  1.0)
            {
                *out_error_ptr = in_path + ".max_far_parallax: expected a number above zero, and up to one (a part of the width of the picture)";

                goto end;
            }

            out_comfort_ptr->max_far_parallax = static_cast<float>(far_ptr->number_value);
        }

        const auto* overrides_ptr = in_value.find_ptr("scene_overrides");

        if (overrides_ptr != nullptr)
        {
            if (parse_scene_overrides(*overrides_ptr,
                                      in_path + ".scene_overrides",
                                      &out_comfort_ptr->scene_override_vec,
                                      out_error_ptr) == false)
            {
                goto end;
            }
        }

        const auto* fraction_ptr = in_value.find_ptr("near_fraction");

        if (fraction_ptr != nullptr)
        {
            const bool is_finite = (::isfinite(fraction_ptr->number_value) != 0);

            if (fraction_ptr->type         != Utils::Json::JsonValueType::NUMBER ||
                is_finite                  == false                              ||
                fraction_ptr->number_value <= 0.0                                ||
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
                is_finite                 == false                              ||
                overlay_ptr->number_value <  0.0                                ||
                overlay_ptr->number_value >  0.5)
            {
                *out_error_ptr = in_path + ".overlay_parallax: expected a number from zero (not pushed) to a half (a part of the width of the picture)";

                goto end;
            }

            out_comfort_ptr->overlay_parallax = static_cast<float>(overlay_ptr->number_value);
        }

        const auto* part_ptr = in_value.find_ptr("part_parallax");

        if (part_ptr != nullptr)
        {
            const bool is_finite = (::isfinite(part_ptr->number_value) != 0);

            if (part_ptr->type         != Utils::Json::JsonValueType::NUMBER ||
                is_finite              == false                              ||
                part_ptr->number_value <  0.0                                ||
                part_ptr->number_value >  0.5)
            {
                *out_error_ptr = in_path + ".part_parallax: expected a number from zero (as much as the overlay parallax) to a half (a part of the width of the picture)";

                goto end;
            }

            out_comfort_ptr->part_parallax = static_cast<float>(part_ptr->number_value);
        }
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
    const auto* pushed_ptr    = in_rule.find_ptr("pushed");
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
                   {"class", "pushed", "signature", "when"},
                   in_path,
                   out_error_ptr) == false)
    {
        goto end;
    }

    if (pushed_ptr != nullptr)
    {
        if (pushed_ptr->type != Utils::Json::JsonValueType::BOOL)
        {
            *out_error_ptr = in_path + ".pushed: expected true or false";

            goto end;
        }

        out_rule_ptr->is_pushed = pushed_ptr->bool_value;
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

        if (constant_ptr               == nullptr                            ||
            constant_ptr->type         != Utils::Json::JsonValueType::STRING ||
            constant_ptr->string_value != "view_matrix")
        {
            *out_error_ptr = when_path + ".constant: expected \"view_matrix\", the only constant a rule can be about";

            goto end;
        }

        if (equals_any_ptr                   == nullptr                           ||
            equals_any_ptr->type             != Utils::Json::JsonValueType::ARRAY ||
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
        *out_error_ptr = in_path + ": expected an object with cover_keys, draw_keys, part_cover_keys, pass_keys and / or pass_draws";

        goto end;
    }

    if (check_keys(in_value,
                   {"cover_keys", "draw_keys", "part_cover_keys", "pass_draws", "pass_keys"},
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
            {"cover_keys", &out_selector_ptr->cover_key_vec},
            {"draw_keys", &out_selector_ptr->draw_key_vec},
            {"part_cover_keys", &out_selector_ptr->part_cover_key_vec},
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

    if (out_selector_ptr->cover_key_vec.empty()      &&
        out_selector_ptr->draw_key_vec.empty ()       &&
        out_selector_ptr->part_cover_key_vec.empty() &&
        out_selector_ptr->pass_draw_vec.empty()      &&
        out_selector_ptr->pass_key_vec.empty () )
    {
        *out_error_ptr = in_path + ": names no draw and no pass";

        goto end;
    }

    result = true;

end:
    return result;
}

bool Analysis::Recipe::RecipeLoader::parse_hex_key(const Utils::Json::JsonValue& in_value,
                                                   const std::string&            in_path,
                                                   uint64_t*                     out_key_ptr,
                                                   std::string*                  out_error_ptr)
{
    constexpr int    HEX_BASE     = 16;
    constexpr size_t N_HEX_DIGITS = 16;

    bool result = false;

    *out_key_ptr = 0;

    if (in_value.type != Utils::Json::JsonValueType::STRING)
    {
        *out_error_ptr = in_path                                           +
                         ": expected a key such as \"0x0123456789abcdef\"";

        goto end;
    }

    {
        const std::string& text = in_value.string_value;

        const size_t n_text_characters = text.size();
        const int    prefix_difference = text.compare(0,
                                                      2,
                                                      "0x");

        if (n_text_characters != 2 + N_HEX_DIGITS ||
            prefix_difference != 0)
        {
            *out_error_ptr = in_path                                                   +
                             ": expected a key such as \"0x0123456789abcdef\", not \"" +
                             text                                                      +
                             "\"";

            goto end;
        }

        for (size_t n_digit = 2;
                    n_digit < text.size();
                  ++n_digit)
        {
            if (::isxdigit(static_cast<unsigned char>(text.at(n_digit) )) == 0)
            {
                *out_error_ptr = in_path                                                   +
                                 ": expected a key such as \"0x0123456789abcdef\", not \"" +
                                 text                                                      +
                                 "\"";

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

bool Analysis::Recipe::RecipeLoader::parse_hex_key_array(const Utils::Json::JsonValue& in_array,
                                                         const std::string&            in_path,
                                                         std::vector<uint64_t>*        out_key_vec_ptr,
                                                         std::string*                  out_error_ptr)
{
    bool result = false;

    out_key_vec_ptr->clear();

    if (in_array.type != Utils::Json::JsonValueType::ARRAY)
    {
        *out_error_ptr = in_path                       +
                         ": expected an array of keys";

        goto end;
    }

    for (size_t n_key = 0;
                n_key < in_array.item_vec.size();
              ++n_key)
    {
        uint64_t          key      = 0;
        const std::string key_path = in_path                     +
                                     "[" + std::to_string(n_key) +
                                     "]";

        if (parse_hex_key(in_array.item_vec.at(n_key),
                          key_path,
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
            is_finite                  == false                              ||
            distance_ptr->number_value <= 0.0                                ||
            distance_ptr->number_value >  static_cast<double>(FLT_MAX) )
        {
            *out_error_ptr = entry_path + "." + in_distance_name + ": expected a number above zero that fits a float";

            goto end;
        }

        for (const auto& other : *out_key_distance_vec_ptr)
        {
            if (other.first == key)
            {
                *out_error_ptr = entry_path       +
                                 "."              +
                                 in_key_name      +
                                 ": the "         +
                                 in_key_name      +
                                 " has a "        +
                                 in_distance_name +
                                 " already";

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

bool Analysis::Recipe::RecipeLoader::parse_matrix(const Utils::Json::JsonValue& in_array,
                                                  const std::string&            in_path,
                                                  std::array<float, 16>*        out_matrix_ptr,
                                                  std::string*                  out_error_ptr)
{
    bool result = false;

    if (in_array.type            != Utils::Json::JsonValueType::ARRAY ||
        in_array.item_vec.size() != out_matrix_ptr->size() )
    {
        *out_error_ptr = in_path                 +
                         ": expected 16 numbers";

        goto end;
    }

    for (size_t n_element = 0;
                n_element < out_matrix_ptr->size();
              ++n_element)
    {
        const auto&  element        = in_array.item_vec.at(n_element);
        const double absolute_value = ::fabs(element.number_value);

        if (element.type    != Utils::Json::JsonValueType::NUMBER ||
            absolute_value  >  static_cast<double>(FLT_MAX) )
        {
            *out_error_ptr = in_path                                  +
                             "["                                      +
                             std::to_string(n_element)                +
                             "]: expected a number that fits a float";

            goto end;
        }

        out_matrix_ptr->at(n_element) = static_cast<float>(element.number_value);
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

        const auto* draw_key_ptr = item.find_ptr("draw_key");
        const auto* pass_key_ptr = item.find_ptr("pass_key");

        if (draw_key_ptr == nullptr ||
            pass_key_ptr == nullptr)
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

bool Analysis::Recipe::RecipeLoader::parse_rule(const Utils::Json::JsonValue& in_rule,
                                                const std::string&            in_path,
                                                MatrixRule*                   out_rule_ptr,
                                                std::string*                  out_error_ptr)
{
    const CallDescriptor* call_ptr = nullptr;
    bool                  result   = false;

    *out_rule_ptr = MatrixRule();

    if (in_rule.type != Utils::Json::JsonValueType::OBJECT)
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

    if (in_rule.at("name").type != Utils::Json::JsonValueType::NULL_VALUE)
    {
        if (in_rule.at("name").type != Utils::Json::JsonValueType::STRING)
        {
            *out_error_ptr = in_path + ".name: expected a string";

            goto end;
        }

        out_rule_ptr->name = in_rule.at("name").string_value;
    }

    if (in_rule.at("call").type != Utils::Json::JsonValueType::STRING)
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

bool Analysis::Recipe::RecipeLoader::parse_scene_overrides(const Utils::Json::JsonValue& in_array,
                                                           const std::string&            in_path,
                                                           std::vector<SceneComfort>*    out_override_vec_ptr,
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
        const auto&       entry      = in_array.item_vec.at(n_entry);
        const std::string entry_path = in_path + "[" + std::to_string(n_entry) + "]";
        SceneComfort      scene_comfort;
        const auto*       shape_ptr  = entry.find_ptr("shape");

        if (entry.type != Utils::Json::JsonValueType::OBJECT)
        {
            *out_error_ptr = entry_path + ": expected an object";

            goto end;
        }

        if (check_keys(entry,
                       {"convergence_distance", "max_far_parallax", "max_near_parallax", "max_separation", "shape"},
                       entry_path,
                       out_error_ptr) == false)
        {
            goto end;
        }

        if (shape_ptr == nullptr)
        {
            *out_error_ptr = entry_path + ".shape is missing";

            goto end;
        }

        if (parse_hex_key(*shape_ptr,
                          entry_path + ".shape",
                         &scene_comfort.shape_key,
                          out_error_ptr) == false)
        {
            goto end;
        }

        for (const auto& other : *out_override_vec_ptr)
        {
            if (other.shape_key == scene_comfort.shape_key)
            {
                *out_error_ptr = entry_path + ".shape: the scene is said twice";

                goto end;
            }
        }

        {
            /* A number field: where its value goes, its name, and its maximum. A fraction of the picture width is at most 1. */
            struct NumberField
            {
                float*      destination_ptr;
                double      max_value;
                const char* name_ptr;
            };

            const NumberField field_array[] =
            {
                {&scene_comfort.convergence_distance, static_cast<double>(FLT_MAX), "convergence_distance"},
                {&scene_comfort.max_far_parallax,     1.0,                          "max_far_parallax"},
                {&scene_comfort.max_near_parallax,    1.0,                          "max_near_parallax"},
                {&scene_comfort.max_separation,       static_cast<double>(FLT_MAX), "max_separation"}
            };

            for (const auto& field : field_array)
            {
                const auto* number_ptr = entry.find_ptr(field.name_ptr);

                if (number_ptr != nullptr)
                {
                    const bool is_finite = (::isfinite(number_ptr->number_value) != 0);

                    if (number_ptr->type         != Utils::Json::JsonValueType::NUMBER ||
                        is_finite                == false                              ||
                        number_ptr->number_value <= 0.0                                ||
                        number_ptr->number_value >  field.max_value)
                    {
                        *out_error_ptr = entry_path + "." + field.name_ptr + ": expected a number above zero (and up to " + std::to_string(field.max_value) + ")";

                        goto end;
                    }

                    *field.destination_ptr = static_cast<float>(number_ptr->number_value);
                }
            }
        }

        out_override_vec_ptr->push_back(scene_comfort);
    }

    result = true;

end:
    return result;
}

bool Analysis::Recipe::RecipeLoader::parse_screen_parallax(const Utils::Json::JsonValue& in_value,
                                                           const std::string&            in_path,
                                                           ScreenParallaxSettings*       out_screen_parallax_ptr,
                                                           std::string*                  out_error_ptr)
{
    /* An angle of view of 180 degrees or more cannot form a picture, so it is refused. */
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

        /* The distance is required, because for pre-transformed vertices there is no other way to find it. */
        if (distance_ptr == nullptr)
        {
            *out_error_ptr = in_path + ".convergence_distance is missing";

            goto end;
        }

        bool is_distance_valid = false;

        if (distance_ptr->type == Utils::Json::JsonValueType::NUMBER)
        {
            const bool is_distance_finite = (::isfinite(distance_ptr->number_value) != 0);

            is_distance_valid = (is_distance_finite &&
                                 distance_ptr->number_value > 0.0);
        }

        if (is_distance_valid == false)
        {
            *out_error_ptr = in_path + ".convergence_distance: expected a number above zero (the distance at which the eyes converge, in the "
                                       "units of the camera of the application)";

            goto end;
        }

        out_screen_parallax_ptr->convergence_distance = static_cast<float>(distance_ptr->number_value);

        if (fov_ptr != nullptr)
        {
            bool is_fov_valid = false;

            if (fov_ptr->type == Utils::Json::JsonValueType::NUMBER)
            {
                const bool is_fov_finite = (::isfinite(fov_ptr->number_value) != 0);

                is_fov_valid = (is_fov_finite                                         &&
                                fov_ptr->number_value > 0.0                           &&
                                fov_ptr->number_value < MAX_FIELD_OF_VIEW_DEGREES);
            }

            if (is_fov_valid == false)
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

bool Analysis::Recipe::RecipeLoader::parse_state(const Utils::Json::JsonValue& in_value,
                                                 const std::string&            in_path,
                                                 const CallDescriptor&         in_call,
                                                 uint32_t*                     out_state_ptr,
                                                 std::string*                  out_error_ptr)
{
    struct StateName
    {
        const char* name;
        uint32_t    value;
    };

    static const StateName D3D7_STATE_NAME_ARRAY[] =
    {
        {"D3DTRANSFORMSTATE_PROJECTION", Interception::Core::D3D7Utils::D3DTRANSFORMSTATE_PROJECTION},
        {"D3DTRANSFORMSTATE_TEXTURE0",   Interception::Core::D3D7Utils::D3DTRANSFORMSTATE_TEXTURE0},
        {"D3DTRANSFORMSTATE_TEXTURE1",   Interception::Core::D3D7Utils::D3DTRANSFORMSTATE_TEXTURE1},
        {"D3DTRANSFORMSTATE_TEXTURE2",   Interception::Core::D3D7Utils::D3DTRANSFORMSTATE_TEXTURE2},
        {"D3DTRANSFORMSTATE_TEXTURE3",   Interception::Core::D3D7Utils::D3DTRANSFORMSTATE_TEXTURE3},
        {"D3DTRANSFORMSTATE_TEXTURE4",   Interception::Core::D3D7Utils::D3DTRANSFORMSTATE_TEXTURE4},
        {"D3DTRANSFORMSTATE_TEXTURE5",   Interception::Core::D3D7Utils::D3DTRANSFORMSTATE_TEXTURE5},
        {"D3DTRANSFORMSTATE_TEXTURE6",   Interception::Core::D3D7Utils::D3DTRANSFORMSTATE_TEXTURE6},
        {"D3DTRANSFORMSTATE_TEXTURE7",   Interception::Core::D3D7Utils::D3DTRANSFORMSTATE_TEXTURE7},
        {"D3DTRANSFORMSTATE_VIEW",       Interception::Core::D3D7Utils::D3DTRANSFORMSTATE_VIEW},
        {"D3DTRANSFORMSTATE_WORLD",      Interception::Core::D3D7Utils::D3DTRANSFORMSTATE_WORLD},
        {"D3DTRANSFORMSTATE_WORLD1",     Interception::Core::D3D7Utils::D3DTRANSFORMSTATE_WORLD1},
        {"D3DTRANSFORMSTATE_WORLD2",     Interception::Core::D3D7Utils::D3DTRANSFORMSTATE_WORLD2},
        {"D3DTRANSFORMSTATE_WORLD3",     Interception::Core::D3D7Utils::D3DTRANSFORMSTATE_WORLD3}
    };

    static const StateName D3D8_STATE_NAME_ARRAY[] =
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

    if (in_value.type == Utils::Json::JsonValueType::NUMBER)
    {
        result = parse_unsigned(in_value,
                                in_path,
                                out_state_ptr,
                                out_error_ptr);
    }
    else
    if (in_value.type == Utils::Json::JsonValueType::STRING)
    {
        const bool       is_d3d7    = (in_call.api == RuleApi::D3D7);
        const StateName* name_array = (is_d3d7) ? D3D7_STATE_NAME_ARRAY
                                                : D3D8_STATE_NAME_ARRAY;
        const size_t     n_names    = (is_d3d7) ? sizeof(D3D7_STATE_NAME_ARRAY) / sizeof(D3D7_STATE_NAME_ARRAY[0])
                                                : sizeof(D3D8_STATE_NAME_ARRAY) / sizeof(D3D8_STATE_NAME_ARRAY[0]);

        for (size_t n_name = 0;
                    n_name < n_names;
                  ++n_name)
        {
            if (in_value.string_value == name_array[n_name].name)
            {
                *out_state_ptr = name_array[n_name].value;

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

bool Analysis::Recipe::RecipeLoader::parse_unsigned(const Utils::Json::JsonValue& in_value,
                                                    const std::string&            in_path,
                                                    uint32_t*                     out_value_ptr,
                                                    std::string*                  out_error_ptr)
{
    const double floor_value = ::floor(in_value.number_value);
    bool         result      = false;

    if (in_value.type         != Utils::Json::JsonValueType::NUMBER ||
        in_value.number_value <  0.0                                ||
        in_value.number_value >  static_cast<double>(UINT32_MAX)    ||
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

bool Analysis::Recipe::RecipeLoader::parse_when(const Utils::Json::JsonValue& in_when,
                                                const std::string&            in_path,
                                                const CallDescriptor&         in_call,
                                                MatrixRule*                   inout_rule_ptr,
                                                std::string*                  out_error_ptr)
{
    bool result = false;

    if (in_when.type != Utils::Json::JsonValueType::OBJECT)
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
                        in_call,
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

        if (matrix.type          != Utils::Json::JsonValueType::STRING ||
            (matrix.string_value != "any"                              &&
             matrix.string_value != "orthographic"                     &&
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
