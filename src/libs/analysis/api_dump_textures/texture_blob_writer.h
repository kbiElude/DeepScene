/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Builds the table of contents and writes the file of blobs (see textures_format.h) for the contents that are not left in the workload. Each
 * content is stored once, however often it is added. Two contents are the same if both hashes and the length match. A content is stored with the
 * filter it was first given. Not thread-safe: use it from one thread at a time.
 */
#ifndef APIDUMPTEXTURES_TEXTURE_BLOB_WRITER_H
#define APIDUMPTEXTURES_TEXTURE_BLOB_WRITER_H

#include "analysis/api_dump_textures/types.h"
#include <fstream>
#include <map>
#include <stdint.h>
#include <string>
#include <tuple>
#include <vector>

namespace Analysis
{
    namespace APIDumpTextures
    {
        class TextureBlobWriter
        {
        public:
            TextureBlobWriter();

            /* Adds the content unless it is already in the table, and returns its index in the table of blobs. Adding the same content again
             * returns the same index.
             */
            bool add(const void*       in_data_ptr,
                     const size_t&     in_n_bytes,
                     const BlobFilter& in_filter,
                     uint32_t*         out_blob_index_ptr,
                     std::string*      out_error_ptr);

            /* Adds a content that stays in the workload, in the data chunk that an upload carried. Nothing is written. The entry records the size
             * and the hashes of the bytes, so that a reader can verify that the chunk it reads is the right one. Adding the same chunk again returns
             * the same index. This needs no file.
             */
            bool add_workload_reference(const void*     in_data_ptr,
                                        const size_t&   in_n_bytes,
                                        const uint32_t& in_data_chunk_id,
                                        uint32_t*       out_blob_index_ptr,
                                        std::string*    out_error_ptr);

            /* Ends the file. */
            bool close(std::string* out_error_ptr);

            const std::vector<BlobEntry>& get_blob_vec() const;

            /* The size of the file so far. */
            uint64_t get_n_bytes() const;

            /* Starts a new file. Any previous content of the file is discarded. */
            bool open(const std::string& in_filename,
                      std::string*       out_error_ptr);

        private:
            /* Identifies a content: the content hash, the length hash and the length. */
            typedef std::tuple<uint64_t, uint64_t, uint64_t> ContentKey;

            std::vector<BlobEntry>            m_blob_vec;
            std::ofstream                     m_file;
            std::map<uint32_t, uint32_t>      m_index_by_chunk_map;
            std::map<ContentKey, uint32_t>    m_index_by_content_map;
            uint64_t                          m_n_bytes;
        };
    }
}

#endif /* APIDUMPTEXTURES_TEXTURE_BLOB_WRITER_H */
