/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "zip_builder.h"
#include "VR/AppInstaller/file_system.h"
#include "miniz.h"
#include <Windows.h>

void Tests::ZipBuilder::add_file(const std::string& in_name,
                                 const std::string& in_content)
{
    m_entry_vec.push_back(std::make_pair(in_name,
                                         in_content) );
}

std::string Tests::ZipBuilder::get_bytes() const
{
    void*          bytes_ptr   = nullptr;
    std::vector<std::pair<std::string, std::string> > placeholder_vec;
    size_t         n_bytes     = 0;
    std::string    result;
    bool           is_ok       = true;
    mz_zip_archive zip;

    mz_zip_zero_struct(&zip);

    if (mz_zip_writer_init_heap(&zip,
                                0,
                                0) == MZ_FALSE)
    {
        goto end;
    }

    for (size_t n_entry = 0;
                n_entry < m_entry_vec.size();
              ++n_entry)
    {
        const std::string& name        = m_entry_vec.at(n_entry).first;
        std::string        placeholder = name;
        mz_bool            is_added    = MZ_FALSE;

        /* miniz does not write the names which an unpacker must refuse (a drive, a slash at the start): such a name is written as a
         * placeholder of the same length, which is replaced in the bytes of the zip file.
         */
        if (name.size() >= 3)
        {
            placeholder             = std::string(name.size(),
                                                  'Q');
            placeholder.at(name.size() - 2) = static_cast<char>('0' + n_entry % 10);

            if (name.at(name.size() - 1) == '/')
            {
                placeholder.at(name.size() - 1) = '/';
            }

            placeholder_vec.push_back(std::make_pair(placeholder,
                                                     name) );
        }

        is_added = mz_zip_writer_add_mem(&zip,
                                         placeholder.c_str(),
                                         m_entry_vec.at(n_entry).second.data(),
                                         m_entry_vec.at(n_entry).second.size(),
                                         MZ_DEFAULT_COMPRESSION);

        is_ok = (is_added != MZ_FALSE) && is_ok;
    }

    if (is_ok &&
        mz_zip_writer_finalize_heap_archive(&zip,
                                           &bytes_ptr,
                                           &n_bytes) != MZ_FALSE)
    {
        result.assign(static_cast<const char*>(bytes_ptr),
                      n_bytes);

        mz_free(bytes_ptr);

        for (size_t n_placeholder = 0;
                    n_placeholder < placeholder_vec.size();
                  ++n_placeholder)
        {
            size_t n_position = result.find(placeholder_vec.at(n_placeholder).first);

            while (n_position != std::string::npos)
            {
                result.replace(n_position,
                               placeholder_vec.at(n_placeholder).first.size(),
                               placeholder_vec.at(n_placeholder).second);

                n_position = result.find(placeholder_vec.at(n_placeholder).first,
                                         n_position + 1);
            }
        }
    }

    mz_zip_writer_end(&zip);

end:
    return result;
}

bool Tests::ZipBuilder::write_file(const std::string& in_path) const
{
    const std::string bytes       = get_bytes();
    HANDLE            file_handle = INVALID_HANDLE_VALUE;
    DWORD             n_written   = 0;
    bool              result      = false;

    if (bytes.empty() )
    {
        goto end;
    }

    file_handle = ::CreateFileW(VR::AppInstaller::FileSystem::to_wide(in_path).c_str(),
                                GENERIC_WRITE,
                                0, /* share mode */
                                nullptr,
                                CREATE_ALWAYS,
                                FILE_ATTRIBUTE_NORMAL,
                                nullptr);

    if (file_handle == INVALID_HANDLE_VALUE)
    {
        goto end;
    }

    result = (::WriteFile(file_handle,
                          bytes.data(),
                          static_cast<DWORD>(bytes.size() ),
                         &n_written,
                          nullptr) != FALSE &&
              n_written == bytes.size() );

    ::CloseHandle(file_handle);

end:
    return result;
}
