/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Names the objects of a D3D8 workload the way APIDumpPrinter does ("Texture#3"), so that what the analysis says about an
 * object can be looked up in the printed calls. Like the printer, it names an address once, and does not forget it when
 * the object is released.
 */
#ifndef APIDUMPANALYSIS_FRONTENDS_OBJECT_NAMER_H
#define APIDUMPANALYSIS_FRONTENDS_OBJECT_NAMER_H

#include <map>
#include <stdint.h>
#include <string>

namespace Analysis
{
    namespace APIDumpAnalysis
    {
        namespace Frontends
        {
            class ObjectNamer
            {
            public:
                ObjectNamer();

                /* "NULL" for no object, the name the object was given, or "<type>@0x<address>" for an object that was never named. */
                std::string get_name(const uint64_t& in_address,
                                     const char*     in_type_name_ptr) const;

                /* Gives the object the next number of its type, if it is new, or has no name yet. */
                void register_object(const uint64_t& in_address,
                                     const char*     in_type_name_ptr,
                                     const bool&     in_is_new);

            private:
                std::map<std::string, uint32_t> m_n_named_objects_map; /* by type name */
                std::map<uint64_t, std::string> m_name_map;            /* by address   */
            };
        }
    }
}

#endif /* APIDUMPANALYSIS_FRONTENDS_OBJECT_NAMER_H */
