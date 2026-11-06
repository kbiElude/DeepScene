/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * What tells a resource from the others in every run of an application, and is known the moment the application makes it: the key of how
 * it was made (see KeyBuilder::compute_description_key) and how many resources with that key were made before it. The key alone is not
 * enough, since an application makes several textures in the same way; the number of the resource is not, since nothing says it is the
 * same in another run. The order in which an application makes its resources is, for the programs that matter here, the same in every run.
 *
 * The keys that the resources have in a frame (see ResourceKeyTable) are not this: they say what wrote to the resource in that frame.
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
            uint32_t ordinal         = 0; /* the number of resources made before it with the same key */
        };

        bool operator==(const ResourceIdentity& in_a,
                        const ResourceIdentity& in_b);

        class ResourceIdentities
        {
        public:
            /* False if there is no such resource in the list (the list is by ID, less 1: Model::resource_vec). */
            static bool find(const std::vector<Resource>& in_resource_vec,
                             const ResourceId&            in_id,
                             ResourceIdentity*            out_identity_ptr);
        };
    }
}

#endif /* APIDUMPANALYSIS_RESOURCE_IDENTITY_H */
