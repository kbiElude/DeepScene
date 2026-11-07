/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/recipe/recipe_loader.h"
#include "interception/core/D3D7Utils/constants.h"
#include "interception/hooks/D3D8/types.h"
#include <stdint.h>

const Analysis::Recipe::RecipeLoader::CallDescriptor* Analysis::Recipe::RecipeLoader::find_call_descriptor_ptr(const std::string& in_name)
{
    /* The functions a rule can apply to: name, function and argument positions (argument 0 is the object it is called on). */
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

bool Analysis::Recipe::RecipeLoader::parse_action(const Utils::Json::JsonValue& in_action,
                                                  const std::string&            in_path,
                                                  Rule*                         inout_rule_ptr,
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

        if (inout_rule_ptr->action_kind != ActionKind::EYE_OFFSET &&
            in_action.find_ptr("axis")  != nullptr)
        {
            *out_error_ptr = in_path + ".axis: only eye_offset has an axis";

            goto end;
        }

        if (inout_rule_ptr->action_kind == ActionKind::EYE_CONVERGENCE)
        {
            if (order_ptr                   != nullptr ||
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

bool Analysis::Recipe::RecipeLoader::parse_rule(const Utils::Json::JsonValue& in_rule,
                                                const std::string&            in_path,
                                                Rule*                         out_rule_ptr,
                                                std::string*                  out_error_ptr)
{
    const CallDescriptor* call_ptr = nullptr;
    bool                  result   = false;

    *out_rule_ptr = Rule();

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

bool Analysis::Recipe::RecipeLoader::parse_when(const Utils::Json::JsonValue& in_when,
                                                const std::string&            in_path,
                                                const CallDescriptor&         in_call,
                                                Rule*                         inout_rule_ptr,
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
