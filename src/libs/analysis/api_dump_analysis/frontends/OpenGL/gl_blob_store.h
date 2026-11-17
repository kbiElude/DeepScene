/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Stores the data that draws read from application memory and exposes it as buffer resources; identical bytes always give the same buffer.
 * Every piece is kept for its frame (drawn once per eye); one-frame pieces are released at frame end, pieces seen in a second frame stay.
 * A released piece seen again is stored again, as every call carries its bytes.
 */
#ifndef APIDUMPANALYSIS_FRONTENDS_OPENGL_GL_BLOB_STORE_H
#define APIDUMPANALYSIS_FRONTENDS_OPENGL_GL_BLOB_STORE_H

#include "analysis/api_dump_analysis/frontends/OpenGL/types.h"
#include "analysis/api_dump_analysis/model_builder.h"
#include <map>
#include <memory>
#include <stddef.h>
#include <stdint.h>
#include <tuple>
#include <vector>

namespace Analysis
{
    namespace APIDumpAnalysis
    {
        namespace Frontends
        {
            class GLBlobStore
            {
            public:
                /* The builder must outlive the store. */
                explicit GLBlobStore(Analysis::APIDumpAnalysis::ModelBuilder* in_builder_ptr);

                /* Returns the bytes of the piece if kept (current-frame pieces and multi-frame pieces); null if not kept or not the store's resource. */
                std::shared_ptr<const std::vector<uint8_t> > get_data_ptr(const Analysis::APIDumpAnalysis::ResourceId& in_resource_id) const;

                /* The number of distinct pieces of data for which the store has created a resource. */
                uint64_t get_n_blobs() const;

                /* The number of bytes currently kept. */
                uint64_t get_n_cached_bytes() const;

                /* Returns the resource for the data, created if these bytes are new for this kind of buffer. Bytes are kept until frame end. */
                Analysis::APIDumpAnalysis::ResourceId get_resource_id(const Analysis::APIDumpAnalysis::ResourceKind& in_kind,
                                                                      const void*                                    in_data_ptr,
                                                                      const size_t&                                  in_n_bytes);

                /* Returns how the piece has been reused so far; ONE_FRAME for a resource that does not belong to the store. */
                GLBlobReuse get_reuse(const Analysis::APIDumpAnalysis::ResourceId& in_resource_id) const;

                /* Ends the frame. Data that was seen only in this frame is released. */
                void on_frame_ended();

            private:
                /* The key of a blob: the kind of buffer, the hash of the bytes and the number of bytes. */
                typedef std::tuple<uint32_t, uint64_t, uint64_t> BlobKey;

                /* Stores the bytes of a blob whose bytes are not yet kept. */
                void cache_bytes(const void*   in_data_ptr,
                                 const size_t& in_n_bytes,
                                 GLBlob*       inout_blob_ptr);

                std::map<BlobKey, GLBlob>                                m_blob_map;
                Analysis::APIDumpAnalysis::ModelBuilder*                 m_builder_ptr;
                std::vector<GLBlob*>                                     m_frame_blob_ptr_vec; /* seen in this frame */
                uint64_t                                                 m_frame_index;
                std::map<Analysis::APIDumpAnalysis::ResourceId, BlobKey> m_key_by_resource_id_map;
                uint64_t                                                 m_n_cached_bytes;
            };
        }
    }
}

#endif /* APIDUMPANALYSIS_FRONTENDS_OPENGL_GL_BLOB_STORE_H */
