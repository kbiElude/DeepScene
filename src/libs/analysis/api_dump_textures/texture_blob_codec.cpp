/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_textures/texture_blob_codec.h"
#include "common/macros.h"
#include "utils/hashing/hasher.h"
#include "zstd.h"

namespace
{
    constexpr int    COMPRESSION_LEVEL = 9;
    constexpr size_t PLANE_STRIDE      = 4;
}

std::vector<uint8_t> Analysis::APIDumpTextures::TextureBlobCodec::apply_filter(const uint8_t*    in_data_ptr,
                                                                               const size_t&     in_n_bytes,
                                                                               const BlobFilter& in_filter)
{
    std::vector<uint8_t> result_vec(in_data_ptr,
                                    in_data_ptr + in_n_bytes);

    switch (in_filter)
    {
        case BlobFilter::BYTE_PLANES_OF_4:
        {
            const size_t n_pixels = in_n_bytes / PLANE_STRIDE;

            for (size_t n_pixel = 0; n_pixel < n_pixels; ++n_pixel)
            {
                for (size_t n_plane = 0; n_plane < PLANE_STRIDE; ++n_plane)
                {
                    result_vec.at(n_plane * n_pixels + n_pixel) = in_data_ptr[n_pixel * PLANE_STRIDE + n_plane];
                }
            }

            break;
        }

        case BlobFilter::NONE:
        {
            break;
        }

        default:
        {
            AI_FAIL_UNSUPPORTED(in_filter);
        }
    }

    return result_vec;
}

void Analysis::APIDumpTextures::TextureBlobCodec::compute_hashes(const void*   in_data_ptr,
                                                                 const size_t& in_n_bytes,
                                                                 uint64_t*     out_content_hash_ptr,
                                                                 uint64_t*     out_length_hash_ptr)
{
    Utils::Hashing::Hasher length_hasher;

    *out_content_hash_ptr = Utils::Hashing::Hasher::hash_bytes(in_data_ptr,
                                                               in_n_bytes);

    length_hasher.add_u64  (in_n_bytes);
    length_hasher.add_bytes(in_data_ptr,
                            in_n_bytes);

    *out_length_hash_ptr = length_hasher.get_digest();
}

bool Analysis::APIDumpTextures::TextureBlobCodec::pack(const void*           in_data_ptr,
                                                       const size_t&         in_n_bytes,
                                                       const BlobFilter&     in_filter,
                                                       std::vector<uint8_t>* out_stored_byte_vec_ptr)
{
    const std::vector<uint8_t> filtered_byte_vec = apply_filter(static_cast<const uint8_t*>(in_data_ptr),
                                                                in_n_bytes,
                                                                in_filter);
    ZSTD_CCtx*                 context_ptr       = ::ZSTD_createCCtx();
    size_t                     n_stored_bytes    = 0;
    bool                       result            = false;

    if (context_ptr == nullptr)
    {
        goto end;
    }

    /* The checksum is what tells a frame that was damaged; the size is in the frame, as the frame is made in one go. */
    ::ZSTD_CCtx_setParameter(context_ptr,
                             ZSTD_c_compressionLevel,
                             COMPRESSION_LEVEL);
    ::ZSTD_CCtx_setParameter(context_ptr,
                             ZSTD_c_checksumFlag,
                             1);

    out_stored_byte_vec_ptr->resize(::ZSTD_compressBound(filtered_byte_vec.size() ) );

    n_stored_bytes = ::ZSTD_compress2(context_ptr,
                                      out_stored_byte_vec_ptr->data(),
                                      out_stored_byte_vec_ptr->size(),
                                      filtered_byte_vec.data(),
                                      filtered_byte_vec.size() );

    if (::ZSTD_isError(n_stored_bytes) )
    {
        out_stored_byte_vec_ptr->clear();

        goto end;
    }

    out_stored_byte_vec_ptr->resize(n_stored_bytes);

    result = true;

end:
    ::ZSTD_freeCCtx(context_ptr);

    return result;
}

void Analysis::APIDumpTextures::TextureBlobCodec::revert_filter(const BlobFilter&     in_filter,
                                                                std::vector<uint8_t>* inout_byte_vec_ptr)
{
    switch (in_filter)
    {
        case BlobFilter::BYTE_PLANES_OF_4:
        {
            const size_t               n_pixels = inout_byte_vec_ptr->size() / PLANE_STRIDE;
            const std::vector<uint8_t> planes   = *inout_byte_vec_ptr;

            for (size_t n_pixel = 0; n_pixel < n_pixels; ++n_pixel)
            {
                for (size_t n_plane = 0; n_plane < PLANE_STRIDE; ++n_plane)
                {
                    inout_byte_vec_ptr->at(n_pixel * PLANE_STRIDE + n_plane) = planes.at(n_plane * n_pixels + n_pixel);
                }
            }

            break;
        }

        case BlobFilter::NONE:
        {
            break;
        }

        default:
        {
            AI_FAIL_UNSUPPORTED(in_filter);
        }
    }
}

bool Analysis::APIDumpTextures::TextureBlobCodec::unpack(const uint8_t*         in_stored_ptr,
                                                         const size_t&          in_n_stored_bytes,
                                                         const BlobEntry&       in_entry,
                                                         std::vector<uint8_t>*  out_raw_byte_vec_ptr,
                                                         std::string*           out_error_ptr)
{
    uint64_t content_hash = 0;
    uint64_t length_hash  = 0;
    size_t   n_raw_bytes  = 0;
    bool     result       = false;

    out_raw_byte_vec_ptr->resize(static_cast<size_t>(in_entry.n_raw_bytes) );

    /* The decompressor checks the checksum of the frame, and the size that the frame says. */
    n_raw_bytes = ::ZSTD_decompress(out_raw_byte_vec_ptr->data(),
                                    out_raw_byte_vec_ptr->size(),
                                    in_stored_ptr,
                                    in_n_stored_bytes);

    if (::ZSTD_isError(n_raw_bytes) ||
        n_raw_bytes != in_entry.n_raw_bytes)
    {
        *out_error_ptr = "A stored content is damaged";

        goto end;
    }

    revert_filter(in_entry.filter,
                  out_raw_byte_vec_ptr);

    compute_hashes(out_raw_byte_vec_ptr->data(),
                   out_raw_byte_vec_ptr->size(),
                  &content_hash,
                  &length_hash);

    if (content_hash != in_entry.content_hash ||
        length_hash  != in_entry.length_hash)
    {
        *out_error_ptr = "A stored content is not the one its entry says";

        goto end;
    }

    result = true;

end:
    if (result == false)
    {
        out_raw_byte_vec_ptr->clear();
    }

    return result;
}
