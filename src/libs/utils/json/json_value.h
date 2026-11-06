/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * A value of a JSON text: what JsonReader gives, and what the readers of the files of this project look into.
 */
#ifndef UTILS_JSON_JSON_VALUE_H
#define UTILS_JSON_JSON_VALUE_H

#include "utils/json/types.h"
#include <string>
#include <vector>

namespace Utils
{
    namespace Json
    {
        struct JsonValue
        {
            bool                     bool_value   = false;
            std::vector<JsonValue>   item_vec;                /* array items, or object values           */
            std::vector<std::string> key_vec;                 /* object keys, parallel to item_vec       */
            double                   number_value = 0.0;
            std::string              string_value;            /* as UTF-8                                */
            JsonValueType            type         = JsonValueType::NULL_VALUE;

            /* The member of an object, or a null value if there is none. */
            const JsonValue& at(const std::string& in_key) const;

            /* The member of an object, or nullptr. */
            const JsonValue* find_ptr(const std::string& in_key) const;

            /* The first key of an object which is not among the allowed ones, or nullptr. */
            const std::string* find_unknown_key_ptr(const std::vector<std::string>& in_allowed_key_vec) const;
        };
    }
}

#endif /* UTILS_JSON_JSON_VALUE_H */
