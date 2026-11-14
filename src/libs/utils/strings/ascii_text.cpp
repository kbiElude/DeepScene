/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "utils/strings/ascii_text.h"
#include <algorithm>

std::string Utils::Strings::AsciiText::to_lower(const std::string& in_text)
{
    std::string result = in_text;

    const auto to_lower_func = [](const char in_character)
    {
        constexpr char CASE_DISTANCE = 'a' - 'A';

        return (in_character >= 'A' && in_character <= 'Z') ? static_cast<char>(in_character + CASE_DISTANCE)
                                                            : in_character;
    };

    std::transform(result.begin(),
                   result.end  (),
                   result.begin(),
                   to_lower_func);

    return result;
}
