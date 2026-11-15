/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Reads contents from the file of blobs (see textures_format.h) one at a time, without reading the whole file. A content that is damaged, or does
 * not match its entry, is refused. Not thread-safe: use it from one thread at a time.
 */
#ifndef APIDUMPTEXTURES_TEXTURE_BLOB_READER_H
#define APIDUMPTEXTURES_TEXTURE_BLOB_READER_H

#include "analysis/api_dump_textures/types.h"
#include <fstream>
#include <stdint.h>
#include <string>
#include <vector>

namespace Analysis
{
    namespace APIDumpTextures
    {
        class TextureBlobReader
        {
        public:
            TextureBlobReader();

            /* The size of the file in bytes. */
            uint64_t get_n_bytes() const;

            bool open(const std::string& in_filename,
                      std::string*       out_error_ptr);

            /* Reads the content that the entry describes. The result holds the bytes exactly as they were given when the content was stored. */
            bool read(const BlobEntry&      in_entry,
                      std::vector<uint8_t>* out_raw_byte_vec_ptr,
                      std::string*          out_error_ptr);

        private:
            std::ifstream m_file;
            uint64_t      m_n_bytes;
        };
    }
}

#endif /* APIDUMPTEXTURES_TEXTURE_BLOB_READER_H */
