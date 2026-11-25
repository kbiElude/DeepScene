/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef APIDUMPANALYSIS_FRONTENDS_OPENGL_GL_CLIENT_ARRAY_RESOURCES_H
#define APIDUMPANALYSIS_FRONTENDS_OPENGL_GL_CLIENT_ARRAY_RESOURCES_H

#include "analysis/api_dump_analysis/model_builder.h"
#include <map>
#include <stddef.h>
#include <stdint.h>
#include <tuple>

namespace Analysis
{
    namespace APIDumpAnalysis
    {
        namespace Frontends
        {
            /* Gives a resource to each different content of the memory of the application that draws read: the vertices and the indices of its arrays.
             * The same bytes are the same resource in any draw or frame. Only the hash of the bytes is kept (in the description of the resource), and
             * not the bytes: nothing reads them after the draw has been described.
             */
            class GLClientArrayResources
            {
            public:
                /* The builder must outlive this object. */
                explicit GLClientArrayResources(Analysis::APIDumpAnalysis::ModelBuilder* in_builder_ptr);

                /* The number of resources made so far. */
                uint64_t get_n_resources() const;

                /* Returns the resource for the data, made if these bytes are new for this kind of buffer. */
                Analysis::APIDumpAnalysis::ResourceId get_resource_id(const Analysis::APIDumpAnalysis::ResourceKind& in_kind,
                                                                      const void*                                    in_data_ptr,
                                                                      const size_t&                                  in_n_bytes);

            private:
                /* The key of a content: the kind of buffer, the hash of the bytes and the number of bytes. */
                typedef std::tuple<uint32_t, uint64_t, uint64_t> ContentKey;

                Analysis::APIDumpAnalysis::ModelBuilder*                    m_builder_ptr;
                std::map<ContentKey, Analysis::APIDumpAnalysis::ResourceId> m_resource_id_map;
            };
        }
    }
}

#endif /* APIDUMPANALYSIS_FRONTENDS_OPENGL_GL_CLIENT_ARRAY_RESOURCES_H */
