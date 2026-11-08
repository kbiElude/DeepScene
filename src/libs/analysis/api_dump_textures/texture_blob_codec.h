/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Makes a content into the bytes that are stored (a filter, then Zstandard with a checksum) and back. Nothing is lost: what unpack() gives is the bytes that
 * pack() was given, and it says so only if the hashes of the entry agree with them.
 */
#ifndef APIDUMPTEXTURES_TEXTURE_BLOB_CODEC_H
#define APIDUMPTEXTURES_TEXTURE_BLOB_CODEC_H

#include "analysis/api_dump_textures/types.h"
#include <stddef.h>
#include <stdint.h>
#include <string>
#include <vector>

namespace Analysis
{
    namespace APIDumpTextures
    {
        class TextureBlobCodec
        {
        public:
            /* The two hashes by which a content is told from the others (see BlobEntry). */
            static void compute_hashes(const void*   in_data_ptr,
                                       const size_t& in_n_bytes,
                                       uint64_t*     out_content_hash_ptr,
                                       uint64_t*     out_length_hash_ptr);

            /* Filters the bytes and compresses them. False if that cannot be done. */
            static bool pack(const void*       in_data_ptr,
                             const size_t&     in_n_bytes,
                             const BlobFilter& in_filter,
                             std::vector<uint8_t>* out_stored_byte_vec_ptr);

            /* Gives the bytes back. False, and the reason, if the stored bytes are damaged or are not those of the entry (size and both hashes). */
            static bool unpack(const uint8_t*   in_stored_ptr,
                               const size_t&    in_n_stored_bytes,
                               const BlobEntry& in_entry,
                               std::vector<uint8_t>* out_raw_byte_vec_ptr,
                               std::string*          out_error_ptr);

        private:
            /* The bytes after the filter, and the bytes before it again. */
            static std::vector<uint8_t> apply_filter  (const uint8_t*    in_data_ptr,
                                                       const size_t&     in_n_bytes,
                                                       const BlobFilter& in_filter);
            static void                 revert_filter (const BlobFilter& in_filter,
                                                       std::vector<uint8_t>* inout_byte_vec_ptr);
        };
    }
}

#endif /* APIDUMPTEXTURES_TEXTURE_BLOB_CODEC_H */
