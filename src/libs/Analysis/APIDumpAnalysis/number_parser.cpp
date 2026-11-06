/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "Analysis/APIDumpAnalysis/number_parser.h"
#include <math.h>
#include <stdlib.h>

bool Analysis::APIDumpAnalysis::NumberParser::parse_double(const std::string& in_text,
                                                           double*            out_value_ptr)
{
    char*      end_ptr   = nullptr;
    bool       is_finite = false;
    bool       result    = false;
    double     value     = 0.0;

    /* strtod skips space and takes a sign, a hexadecimal number, "inf" and "nan": none of them is a plain number. */
    if (in_text.empty            ()                                    ||
        in_text.find_first_not_of("0123456789.") != std::string::npos)
    {
        goto end;
    }

    value = ::strtod(in_text.c_str(),
                    &end_ptr);

    is_finite = (::isfinite(value) != 0);

    if (*end_ptr  != '\0' ||
        is_finite == false)
    {
        goto end;
    }

    *out_value_ptr = value;
    result         = true;

end:
    return result;
}

bool Analysis::APIDumpAnalysis::NumberParser::parse_unsigned(const std::string& in_text,
                                                             const uint64_t&    in_max_value,
                                                             uint64_t*          out_value_ptr)
{
    constexpr uint64_t BASE = 10;

    bool     result = false;
    uint64_t value  = 0;

    if (in_text.empty() )
    {
        goto end;
    }

    for (const char digit : in_text)
    {
        if (digit < '0' ||
            digit > '9')
        {
            goto end;
        }

        const uint64_t n_digit = static_cast<uint64_t>(digit - '0');

        /* value * 10 + digit would pass the maximum. */
        if (n_digit > in_max_value                    ||
            value   > (in_max_value - n_digit) / BASE)
        {
            goto end;
        }

        value = value * BASE + n_digit;
    }

    *out_value_ptr = value;
    result         = true;

end:
    return result;
}
