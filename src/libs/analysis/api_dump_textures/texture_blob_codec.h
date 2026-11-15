/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Converts a content to the bytes that are stored (a filter, then Zstandard with a checksum) and back. The conversion is lossless: unpack() returns
 * the bytes that pack() was given, and it succeeds only if they match the hashes of the entry.
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
            /* Computes the two hashes that identify a content (see BlobEntry): the hash of the bytes, and the hash of the length followed by the bytes. */
            static void compute_hashes(const void*   in_data_ptr,
                                       const size_t& in_n_bytes,
                                       uint64_t*     out_content_hash_ptr,
                                       uint64_t*     out_length_hash_ptr);

            /* Applies the filter and compresses the bytes. Returns false if that fails. */
            static bool pack(const void*           in_data_ptr,
                             const size_t&         in_n_bytes,
                             const BlobFilter&     in_filter,
                             std::vector<uint8_t>* out_stored_byte_vec_ptr);

            /* Decompresses the stored bytes and reverts the filter. Returns false, with the reason in the error, if the stored bytes are damaged
             * or do not match the entry (size and both hashes).
             */
            static bool unpack(const uint8_t*        in_stored_ptr,
                               const size_t&         in_n_stored_bytes,
                               const BlobEntry&      in_entry,
                               std::vector<uint8_t>* out_raw_byte_vec_ptr,
                               std::string*          out_error_ptr);

        private:
            /* The size in bytes of the pixel whose bytes the filter splits into planes. */
            static const size_t PLANE_STRIDE;

            /* apply_filter returns the bytes after the filter. revert_filter turns filtered bytes back into the original bytes, in place. */
            static std::vector<uint8_t> apply_filter (const uint8_t*        in_data_ptr,
                                                      const size_t&         in_n_bytes,
                                                      const BlobFilter&     in_filter);
            static void                 revert_filter(const BlobFilter&     in_filter,
                                                      std::vector<uint8_t>* inout_byte_vec_ptr);
        };
    }
}

#endif /* APIDUMPTEXTURES_TEXTURE_BLOB_CODEC_H */
