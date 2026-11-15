/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * What tells a resource apart from the others in every run of an application, and is known at the moment the application creates it: the key
 * of how it was created (see MatchKeyBuilder::compute_description_key) and the number of resources with that key that were created before it. The
 * key alone is not enough, because an application creates several textures in the same way. The resource number is not enough either,
 * because nothing guarantees that it is the same in another run. The order in which an application creates its resources is the same in every
 * run, for the programs that matter here.
 *
 * This is not the key that a resource has in a frame (see ResourceKeyTable): that key says what wrote to the resource in that frame.
 */
#ifndef APIDUMPANALYSIS_RESOURCE_IDENTITY_H
#define APIDUMPANALYSIS_RESOURCE_IDENTITY_H

#include "analysis/api_dump_analysis/types.h"
#include <stdint.h>
#include <vector>

namespace Analysis
{
    namespace APIDumpAnalysis
    {
        struct ResourceIdentity
        {
            uint64_t description_key = 0;
            uint32_t ordinal         = 0; /* the number of resources with the same key that were created before this one */
        };

        bool operator==(const ResourceIdentity& in_a,
                        const ResourceIdentity& in_b);

        class ResourceIdentities
        {
        public:
            /* Returns false if the list has no such resource (the list is indexed by ID - 1: Model::resource_vec). */
            static bool find(const std::vector<Resource>& in_resource_vec,
                             const ResourceId&            in_id,
                             ResourceIdentity*            out_identity_ptr);
        };
    }
}

#endif /* APIDUMPANALYSIS_RESOURCE_IDENTITY_H */
