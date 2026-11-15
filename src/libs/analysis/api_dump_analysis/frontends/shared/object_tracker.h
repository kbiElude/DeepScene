/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * The live D3D8 objects by address, and what each one is in the model: a resource, or a part of a resource (the surface of one mip level of
 * a texture). An address is forgotten when its object is released, so that a later object created at the same address is not mistaken for
 * the old one.
 */
#ifndef APIDUMPANALYSIS_FRONTENDS_OBJECT_TRACKER_H
#define APIDUMPANALYSIS_FRONTENDS_OBJECT_TRACKER_H

#include "analysis/api_dump_analysis/types.h"
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
                    bool                                    owns_resource = false; /* true: the object is a resource; false: a part of one */
                    Analysis::APIDumpAnalysis::ResourceView view;                  /* the resource, or the part of it                      */
                };

                ObjectTracker();

                void forget(const uint64_t& in_address);

                size_t get_n_objects() const;

                /* Returns nullptr if the address does not belong to a live, tracked object. */
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
