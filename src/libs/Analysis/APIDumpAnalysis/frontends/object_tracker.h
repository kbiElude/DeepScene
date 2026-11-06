/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * The D3D8 objects that are alive, by address, and what each of them is in the model: a resource, or a part of one (the
 * surface of a mip level of a texture). An address is forgotten when its object is released, so that an object created
 * at the same address later is not taken for the old one.
 */
#ifndef APIDUMPANALYSIS_FRONTENDS_OBJECT_TRACKER_H
#define APIDUMPANALYSIS_FRONTENDS_OBJECT_TRACKER_H

#include "Analysis/APIDumpAnalysis/types.h"
#include <map>
#include <stddef.h>
#include <stdint.h>

namespace Analysis
{
    namespace APIDumpAnalysis
    {
        namespace Frontends
        {
            class ObjectTracker
            {
            public:
                struct TrackedObject
                {
                    bool                                    owns_resource = false; /* the object is a resource, not a part of one */
                    Analysis::APIDumpAnalysis::ResourceView view;                  /* the resource, or the part of it             */
                };

                ObjectTracker();

                void forget(const uint64_t& in_address);

                size_t get_n_objects() const;

                /* nullptr if the address is not that of an object which is alive and tracked. */
                const TrackedObject* get_object_ptr(const uint64_t& in_address) const;

                void track(const uint64_t&      in_address,
                           const TrackedObject& in_object);

            private:
                std::map<uint64_t, TrackedObject> m_object_map;
            };
        }
    }
}

#endif /* APIDUMPANALYSIS_FRONTENDS_OBJECT_TRACKER_H */
