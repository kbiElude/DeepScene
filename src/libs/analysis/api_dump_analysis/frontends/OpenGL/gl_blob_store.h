/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * The data that draws read from the memory of the application, as resources of the model: a piece of data is a buffer which the model builder
 * is told of the first time it is seen, and is the same buffer each time after that, in whatever frame, for as long as its bytes are the same.
 *
 * The bytes are kept as long as they are worth keeping, which depends on how the piece is used again. Every piece is kept for the frame that it
 * is seen in (so that a frame can be drawn once for each eye); a piece that was seen in only one frame is let go of at the end of it, and one that
 * is seen in a second frame is reuse across frames, and is kept on. A piece that was let go of and is seen again is kept again, as a call has
 * its bytes each time.
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

                /* The bytes of the piece if they are kept (they are for every piece of this frame, and for those seen in more than one frame);
                 * null if they are not, or the resource is not one of the store.
                 */
                std::shared_ptr<const std::vector<uint8_t> > get_data_ptr(const Analysis::APIDumpAnalysis::ResourceId& in_resource_id) const;

                /* The number of different pieces of data that the store has made a resource of. */
                uint64_t get_n_blobs() const;

                /* The number of bytes that are kept now. */
                uint64_t get_n_cached_bytes() const;

                /* The resource of the data: made now if the store has not seen these bytes (as this kind of buffer) before. The bytes are kept
                 * at least until the end of the frame.
                 */
                Analysis::APIDumpAnalysis::ResourceId get_resource_id(const Analysis::APIDumpAnalysis::ResourceKind& in_kind,
                                                                      const void*                                    in_data_ptr,
                                                                      const size_t&                                  in_n_bytes);

                /* How the piece has been used again so far; ONE_FRAME for a resource that is not one of the store. */
                GLBlobReuse get_reuse(const Analysis::APIDumpAnalysis::ResourceId& in_resource_id) const;

                /* The frame is over: what was seen in it only is let go of. */
                void on_frame_ended();

            private:
                /* The kind of buffer, the hash of the bytes and their number. */
                typedef std::tuple<uint32_t, uint64_t, uint64_t> BlobKey;

                /* Keeps the bytes of the blob, which are not kept. */
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
