/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "utils/files/file_writer.h"
#include <stdio.h>

bool Utils::Files::FileWriter::write_bytes(const std::string& in_path,
                                           const void*        in_data_ptr,
                                           const size_t&      in_n_bytes)
{
    FILE* const file_handle = ::fopen(in_path.c_str(),
                                      "wb");
    bool        is_written  = false;

    if (file_handle != nullptr)
    {
        is_written = (in_n_bytes == 0 ||
                      ::fwrite(in_data_ptr,
                               in_n_bytes,
                               1,
                               file_handle) == 1);

        const int close_result = ::fclose(file_handle);

        is_written = (is_written &&
                      close_result == 0);
    }

    return is_written;
}
