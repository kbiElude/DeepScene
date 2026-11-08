/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Reads and writes .textures files (see textures_format.h): the timeline, not the bytes of the blobs, which are in the file beside it (see
 * TextureBlobWriter). The same timeline always gives the same bytes.
 */
#ifndef APIDUMPTEXTURES_TEXTURES_FILE_H
#define APIDUMPTEXTURES_TEXTURES_FILE_H

#include "analysis/api_dump_textures/types.h"
#include <stdint.h>
#include <string>
#include <vector>

namespace Analysis
{
    namespace APIDumpTextures
    {
        class TexturesFile
        {
        public:
            /* The name of the file of blobs that goes with a .textures file. */
            static std::string get_blob_filename(const std::string& in_textures_filename);

            /* The error is set when the function returns false. */
            static bool read(const std::string& in_filename,
                             TextureTimeline*   out_timeline_ptr,
                             std::string*       out_error_ptr);

            static bool                 read_bytes (const std::vector<uint8_t>& in_byte_vec,
                                                    TextureTimeline*            out_timeline_ptr,
                                                    std::string*                out_error_ptr);
            static bool                 write      (const std::string&          in_filename,
                                                    const TextureTimeline&      in_timeline,
                                                    std::string*                out_error_ptr);
            static std::vector<uint8_t> write_bytes(const TextureTimeline&      in_timeline);
        };
    }
}

#endif /* APIDUMPTEXTURES_TEXTURES_FILE_H */
