/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "Analysis/APIDumpAnalysis/frontends/object_namer.h"
#include <stdio.h>

Analysis::APIDumpAnalysis::Frontends::ObjectNamer::ObjectNamer()
{
    /* Stub */
}

std::string Analysis::APIDumpAnalysis::Frontends::ObjectNamer::get_name(const uint64_t& in_address,
                                                                        const char*     in_type_name_ptr) const
{
    constexpr size_t ADDRESS_TEXT_N_BYTES = 3 + 16 + 1; /* "@0x", 16 hex digits and NUL */

    const auto  name_iterator = m_name_map.find(in_address);
    std::string result;

    if (in_address == 0)
    {
        result = "NULL";
    }
    else
    if (name_iterator != m_name_map.end() )
    {
        result = name_iterator->second;
    }
    else
    {
        char address_i8vec20[ADDRESS_TEXT_N_BYTES];

        snprintf(address_i8vec20,
                 sizeof(address_i8vec20),
                 "@0x%llx",
                 static_cast<unsigned long long>(in_address) );

        result = std::string((in_type_name_ptr != nullptr) ? in_type_name_ptr
                                                           : "Object") + address_i8vec20;
    }

    return result;
}

void Analysis::APIDumpAnalysis::Frontends::ObjectNamer::register_object(const uint64_t& in_address,
                                                                        const char*     in_type_name_ptr,
                                                                        const bool&     in_is_new)
{
    if (in_address                   != 0                &&
        (in_is_new                                       ||
         m_name_map.find(in_address) == m_name_map.end() ))
    {
        const std::string type_name(in_type_name_ptr);
        const auto        n_object = ++m_n_named_objects_map[type_name];

        m_name_map[in_address] = type_name + "#" + std::to_string(n_object);
    }
}
