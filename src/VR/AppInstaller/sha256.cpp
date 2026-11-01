/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "VR/AppInstaller/file_system.h"
#include "VR/AppInstaller/sha256.h"
#include <Windows.h>
#include <bcrypt.h>
#include <stdint.h>
#include <vector>

VR::AppInstaller::Sha256::Sha256()
    :m_algorithm_handle_ptr(nullptr),
     m_hash_handle_ptr     (nullptr)
{
    BCRYPT_ALG_HANDLE  algorithm_handle = nullptr;
    BCRYPT_HASH_HANDLE hash_handle      = nullptr;

    if (::BCryptOpenAlgorithmProvider(&algorithm_handle,
                                      BCRYPT_SHA256_ALGORITHM,
                                      nullptr,
                                      0) == 0)
    {
        m_algorithm_handle_ptr = algorithm_handle;

        if (::BCryptCreateHash(algorithm_handle,
                              &hash_handle,
                               nullptr,
                               0,
                               nullptr,
                               0,
                               0) == 0)
        {
            m_hash_handle_ptr = hash_handle;
        }
    }
}

VR::AppInstaller::Sha256::~Sha256()
{
    if (m_hash_handle_ptr != nullptr)
    {
        ::BCryptDestroyHash(static_cast<BCRYPT_HASH_HANDLE>(m_hash_handle_ptr) );
    }

    if (m_algorithm_handle_ptr != nullptr)
    {
        ::BCryptCloseAlgorithmProvider(static_cast<BCRYPT_ALG_HANDLE>(m_algorithm_handle_ptr),
                                       0);
    }
}

bool VR::AppInstaller::Sha256::finish(std::string* out_hex_ptr)
{
    static const char DIGIT_ARRAY[] = "0123456789abcdef";

    std::vector<uint8_t> digest_vec(32);
    bool                 result = false;

    if (m_hash_handle_ptr == nullptr)
    {
        goto end;
    }

    if (::BCryptFinishHash(static_cast<BCRYPT_HASH_HANDLE>(m_hash_handle_ptr),
                          &digest_vec.at(0),
                           static_cast<ULONG>(digest_vec.size() ),
                           0) != 0)
    {
        goto end;
    }

    out_hex_ptr->clear();

    for (size_t n_byte = 0;
                n_byte < digest_vec.size();
              ++n_byte)
    {
        *out_hex_ptr += DIGIT_ARRAY[digest_vec.at(n_byte) >> 4];
        *out_hex_ptr += DIGIT_ARRAY[digest_vec.at(n_byte) & 15];
    }

    result = true;

end:
    return result;
}

bool VR::AppInstaller::Sha256::hash_bytes(const void*   in_bytes_ptr,
                                          const size_t& in_n_bytes,
                                          std::string*  out_hex_ptr)
{
    Sha256 hasher;
    bool   result = false;

    if (hasher.update(in_bytes_ptr,
                      in_n_bytes) == false)
    {
        goto end;
    }

    result = hasher.finish(out_hex_ptr);

end:
    return result;
}

bool VR::AppInstaller::Sha256::hash_file(const std::string&       in_path,
                                         const std::atomic<bool>& in_is_cancelled,
                                         std::string*             out_hex_ptr,
                                         std::string*             out_error_ptr)
{
    constexpr size_t CHUNK_N_BYTES = 64 * 1024;

    std::vector<uint8_t> buffer_vec(CHUNK_N_BYTES);
    HANDLE               file_handle  = INVALID_HANDLE_VALUE;
    Sha256               hasher;
    DWORD                n_read_bytes = 0;
    bool                 result       = false;

    file_handle = ::CreateFileW(FileSystem::to_wide(in_path).c_str(),
                                GENERIC_READ,
                                FILE_SHARE_READ,
                                nullptr,
                                OPEN_EXISTING,
                                FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
                                nullptr);

    if (file_handle == INVALID_HANDLE_VALUE)
    {
        *out_error_ptr = "Cannot open " + in_path + " (error " + std::to_string(::GetLastError() ) + ")";

        goto end;
    }

    do
    {
        const BOOL is_read = ::ReadFile(file_handle,
                                       &buffer_vec.at(0),
                                        static_cast<DWORD>(buffer_vec.size() ),
                                       &n_read_bytes,
                                        nullptr);

        if (is_read == FALSE)
        {
            *out_error_ptr = "Cannot read " + in_path + " (error " + std::to_string(::GetLastError() ) + ")";

            goto end;
        }

        if (in_is_cancelled.load() )
        {
            *out_error_ptr = "Cancelled";

            goto end;
        }

        if (n_read_bytes > 0)
        {
            if (hasher.update(&buffer_vec.at(0),
                              n_read_bytes) == false)
            {
                *out_error_ptr = "The hashing of the system cannot be used";

                goto end;
            }
        }
    }
    while (n_read_bytes > 0);

    result = hasher.finish(out_hex_ptr);

    if (result == false)
    {
        *out_error_ptr = "The hashing of the system cannot be used";
    }

end:
    if (file_handle != INVALID_HANDLE_VALUE)
    {
        ::CloseHandle(file_handle);
    }

    return result;
}

bool VR::AppInstaller::Sha256::update(const void*   in_bytes_ptr,
                                      const size_t& in_n_bytes)
{
    bool result = false;

    if (m_hash_handle_ptr == nullptr)
    {
        goto end;
    }

    result = (::BCryptHashData(static_cast<BCRYPT_HASH_HANDLE>(m_hash_handle_ptr),
                               reinterpret_cast<PUCHAR>(const_cast<void*>(in_bytes_ptr) ),
                               static_cast<ULONG>(in_n_bytes),
                               0) == 0);

end:
    return result;
}
