/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "Analysis/APIDumpAnalysis/resource_identity.h"

bool Analysis::APIDumpAnalysis::operator==(const ResourceIdentity& in_a,
                                           const ResourceIdentity& in_b)
{
    return (in_a.description_key == in_b.description_key &&
            in_a.ordinal         == in_b.ordinal);
}

bool Analysis::APIDumpAnalysis::ResourceIdentities::find(const std::vector<Resource>& in_resource_vec,
                                                         const ResourceId&            in_id,
                                                         ResourceIdentity*            out_identity_ptr)
{
    const uint32_t id_value = get_resource_id_value(in_id);
    bool           result   = (id_value >= 1 &&
                               id_value <= in_resource_vec.size() );

    if (result)
    {
        const uint64_t description_key = in_resource_vec.at(id_value - 1).match_key;
        uint32_t       ordinal         = 0;

        for (uint32_t n_resource = 0;
                      n_resource < id_value - 1;
                    ++n_resource)
        {
            if (in_resource_vec.at(n_resource).match_key == description_key)
            {
                ordinal++;
            }
        }

        out_identity_ptr->description_key = description_key;
        out_identity_ptr->ordinal         = ordinal;
    }

    return result;
}
