/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * The enums of the JSON values.
 */
#ifndef UTILS_JSON_TYPES_H
#define UTILS_JSON_TYPES_H

namespace Utils
{
    namespace Json
    {
        enum class JsonValueType
        {
            ARRAY,
            BOOL,
            NULL_VALUE,
            NUMBER,
            OBJECT,
            STRING
        };
    }
}

#endif /* UTILS_JSON_TYPES_H */
