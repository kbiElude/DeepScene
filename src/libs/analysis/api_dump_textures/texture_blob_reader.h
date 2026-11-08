/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Reads the contents from the file of blobs (see textures_format.h), any one alone: the file is not read as a whole. A content that is damaged, or is not
 * the one its entry says, is refused. One thread at a time.
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

            /* The size of the file. */
            uint64_t get_n_bytes() const;

            bool open(const std::string& in_filename,
                      std::string*       out_error_ptr);

            /* The bytes of the content, as they were given when it was stored. */
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
