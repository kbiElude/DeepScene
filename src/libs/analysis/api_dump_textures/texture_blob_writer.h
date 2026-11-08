/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Makes the table of contents, and writes the file of blobs (see textures_format.h) for those that are not left in the workload: each content once, whatever the number of times it is added. A content is told from the others by its two
 * hashes and its length; it is stored with the filter that it was first given. One thread at a time.
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

            /* Adds the content, if it is not there, and gives its place in the table of blobs; the second time the same content is added it is the same place. */
            bool add(const void*       in_data_ptr,
                     const size_t&     in_n_bytes,
                     const BlobFilter& in_filter,
                     uint32_t*         out_blob_index_ptr,
                     std::string*      out_error_ptr);

            /* Adds a content that is left where it is in the workload (the data chunk that an upload carried): nothing is written, and the entry has the size and the
             * hashes of the bytes, so that a reader can tell the chunk it reads is the one. The place of a chunk that is added again is the same. Needs no file.
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

            /* Starts the file (what was in it is gone). */
            bool open(const std::string& in_filename,
                      std::string*       out_error_ptr);

        private:
            /* The content hash, the length hash and the length. */
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
