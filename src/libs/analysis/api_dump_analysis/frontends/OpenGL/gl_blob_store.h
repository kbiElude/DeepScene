/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * The data that draws read from the memory of the application, as resources of the model: a piece of data is a buffer which the model builder
 * is told of the first time it is seen, and is the same buffer each time after that, in whatever frame, for as long as its bytes are the same.
 * Only the hash and the size of a piece are kept, not its bytes.
 */
#ifndef APIDUMPANALYSIS_FRONTENDS_OPENGL_GL_BLOB_STORE_H
#define APIDUMPANALYSIS_FRONTENDS_OPENGL_GL_BLOB_STORE_H

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
            class GlBlobStore
            {
            public:
                /* The builder must outlive the store. */
                explicit GlBlobStore(Analysis::APIDumpAnalysis::ModelBuilder* in_builder_ptr);

                /* The number of different pieces of data that the store has made a resource of. */
                uint64_t get_n_blobs() const;

                /* The resource of the data: made now if the store has not seen these bytes (as this kind of buffer) before. */
                Analysis::APIDumpAnalysis::ResourceId get_resource_id(const Analysis::APIDumpAnalysis::ResourceKind& in_kind,
                                                                      const void*                                    in_data_ptr,
                                                                      const size_t&                                  in_n_bytes);

            private:
                /* The kind of buffer, the hash of the bytes and their number. */
                typedef std::tuple<uint32_t, uint64_t, uint64_t> BlobKey;

                Analysis::APIDumpAnalysis::ModelBuilder*                 m_builder_ptr;
                std::map<BlobKey, Analysis::APIDumpAnalysis::ResourceId> m_resource_id_map;
            };
        }
    }
}

#endif /* APIDUMPANALYSIS_FRONTENDS_OPENGL_GL_BLOB_STORE_H */
