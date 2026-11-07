/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_analysis/passes_format.h"
#include "analysis/recipe/recipe_loader.h"
#include <algorithm>
#include <ctype.h>
#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>

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

bool Analysis::Recipe::RecipeLoader::has_common_key(const std::vector<uint64_t>& in_a,
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
        {&in_recipe.skipped,    "skipped"   },
        {&in_recipe.unaffected, "unaffected"}
    };
    std::vector<std::string> result;

    for (const auto& named : named_array)
    {
        if (named.selector_ptr->pass_key_vec.empty() == false)
        {
            result.push_back(std::string("\"") + named.name + ".pass_keys\" names " + std::to_string(named.selector_ptr->pass_key_vec.size() ) + " passes: a converter that plays the application as it runs does not know passes, and leaves them out");
        }

        if (named.selector_ptr->pass_draw_vec.empty() == false)
        {
            result.push_back(std::string("\"") + named.name + ".pass_draws\" names " + std::to_string(named.selector_ptr->pass_draw_vec.size() ) + " draws in a pass: a converter that plays the application as it runs does not know passes, and leaves them out");
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
                   {"affected", "baked", "comfort", "key_scheme_version", "known_shape_keys", "on_unknown_frame", "recipe_version", "rules", "screen_parallax", "skipped", "target", "unaffected"},
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
        const auto* screen_parallax_ptr    = root.find_ptr("screen_parallax");
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
            Rule              rule;

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

        if (text.size   ()           != 2 + N_HEX_DIGITS ||
            text.compare(0, 2, "0x") != 0)
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
