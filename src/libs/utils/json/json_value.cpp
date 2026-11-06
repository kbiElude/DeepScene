/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "utils/json/json_value.h"
#include <algorithm>

const Utils::Json::JsonValue& Utils::Json::JsonValue::at(const std::string& in_key) const
{
    static const JsonValue empty_value;

    const JsonValue* value_ptr = find_ptr(in_key);

    return (value_ptr != nullptr) ? *value_ptr
                                  : empty_value;
}

const Utils::Json::JsonValue* Utils::Json::JsonValue::find_ptr(const std::string& in_key) const
{
    const JsonValue* result_ptr = nullptr;

    for (size_t n_key = 0;
                n_key < key_vec.size();
              ++n_key)
    {
        if (key_vec.at(n_key) == in_key)
        {
            result_ptr = &item_vec.at(n_key);

            break;
        }
    }

    return result_ptr;
}

const std::string* Utils::Json::JsonValue::find_unknown_key_ptr(const std::vector<std::string>& in_allowed_key_vec) const
{
    const std::string* result_ptr = nullptr;

    for (const auto& key : key_vec)
    {
        if (std::find(in_allowed_key_vec.begin(),
                      in_allowed_key_vec.end  (),
                      key) == in_allowed_key_vec.end() )
        {
            result_ptr = &key;

            break;
        }
    }

    return result_ptr;
}
