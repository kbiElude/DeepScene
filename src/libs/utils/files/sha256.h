/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef UTILS_FILES_SHA256_H
#define UTILS_FILES_SHA256_H

#include <atomic>
#include <stddef.h>
#include <string>

namespace Utils
{
    namespace Files
    {
        /* SHA-256, with the cryptography of the system (CNG): what a download is compared with before it is trusted. One object
         * hashes one run of bytes, which are given to it in pieces.
         */
        class Sha256
        {
        public:
            Sha256();
            ~Sha256();

            Sha256(const Sha256&)            = delete;
            Sha256& operator=(const Sha256&) = delete;

            /* The hash as 64 characters of hex in lower case. After it, the object is done. */
            bool finish(std::string* out_hex_ptr);

            /* Adds bytes to what is hashed. Returns false if the hashing of the system cannot be used. */
            bool update(const void*   in_bytes_ptr,
                        const size_t& in_n_bytes);

            /* The hash of bytes which are in memory. */
            static bool hash_bytes(const void*   in_bytes_ptr,
                                   const size_t& in_n_bytes,
                                   std::string*  out_hex_ptr);

            /* The hash of a file. Returns false, and says why, if the file cannot be read, or if in_is_cancelled is set while it is. */
            static bool hash_file(const std::string&       in_path,
                                  const std::atomic<bool>& in_is_cancelled,
                                  std::string*             out_hex_ptr,
                                  std::string*             out_error_ptr);

        private:
            void* m_algorithm_handle_ptr; /* BCRYPT_ALG_HANDLE  */
            void* m_hash_handle_ptr;      /* BCRYPT_HASH_HANDLE */
        };
    }
}

#endif /* UTILS_FILES_SHA256_H */
