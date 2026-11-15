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
        /* SHA-256 computed with the Windows cryptography API (CNG). A download is compared against it before it is trusted. One object hashes
         * one sequence of bytes, which is supplied in pieces.
         */
        class Sha256
        {
        public:
            Sha256();
            ~Sha256();

            Sha256(const Sha256&)            = delete;
            Sha256& operator=(const Sha256&) = delete;

            /* Returns the hash as 64 lower-case hex characters. The object cannot be used afterwards. */
            bool finish(std::string* out_hex_ptr);

            /* Adds bytes to the data being hashed. Returns false if the system hashing API cannot be used. */
            bool update(const void*   in_bytes_ptr,
                        const size_t& in_n_bytes);


            /* Hashes bytes that are already in memory. */
            static bool hash_bytes(const void*   in_bytes_ptr,
                                   const size_t& in_n_bytes,
                                   std::string*  out_hex_ptr);

            /* Hashes a file. Returns false and sets the error if the file cannot be read or if in_is_cancelled is set while it is being read. */
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
