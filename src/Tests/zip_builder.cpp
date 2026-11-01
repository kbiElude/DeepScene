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
    bool                                              are_all_entries_added = true;
    void*                                             bytes_ptr             = nullptr;
    size_t                                            n_zip_file_bytes      = 0;
    std::vector<std::pair<std::string, std::string> > placeholder_vec;
    std::string                                       result;
    mz_zip_archive                                    zip_archive;

    mz_zip_zero_struct(&zip_archive);

    if (mz_zip_writer_init_heap(&zip_archive,
                                0,
                                0) == MZ_FALSE)
    {
        goto end;
    }

    for (size_t n_entry = 0;
                n_entry < m_entry_vec.size();
              ++n_entry)
    {
        const std::string& entry_name     = m_entry_vec.at(n_entry).first;
        mz_bool            is_entry_added = MZ_FALSE;
        std::string        placeholder    = entry_name;

        /* miniz does not write the names which an unpacker must refuse (a drive, a slash at the start): such a name is written as a
         * placeholder of the same length, which is replaced in the bytes of the zip file.
         */
        if (entry_name.size() >= 3)
        {
            placeholder             = std::string(entry_name.size(),
                                                  'Q');
            placeholder.at(entry_name.size() - 2) = static_cast<char>('0' + n_entry % 10);

            if (entry_name.at(entry_name.size() - 1) == '/')
            {
                placeholder.at(entry_name.size() - 1) = '/';
            }

            placeholder_vec.push_back(std::make_pair(placeholder,
                                                     entry_name) );
        }

        is_entry_added = mz_zip_writer_add_mem(&zip_archive,
                                               placeholder.c_str(),
                                               m_entry_vec.at(n_entry).second.data(),
                                               m_entry_vec.at(n_entry).second.size(),
                                               MZ_DEFAULT_COMPRESSION);

        are_all_entries_added = (is_entry_added != MZ_FALSE) && are_all_entries_added;
    }

    if (are_all_entries_added &&
        mz_zip_writer_finalize_heap_archive(&zip_archive,
                                           &bytes_ptr,
                                           &n_zip_file_bytes) != MZ_FALSE)
    {
        result.assign(static_cast<const char*>(bytes_ptr),
                      n_zip_file_bytes);

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

    mz_zip_writer_end(&zip_archive);

end:
    return result;
}

bool Tests::ZipBuilder::write_file(const std::string& in_path) const
{
    HANDLE            file_handle     = INVALID_HANDLE_VALUE;
    DWORD             n_bytes_written = 0;
    bool              result          = false;
    const std::string zip_file_bytes  = get_bytes();

    if (zip_file_bytes.empty() )
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
                          zip_file_bytes.data(),
                          static_cast<DWORD>(zip_file_bytes.size() ),
                         &n_bytes_written,
                          nullptr) != FALSE &&
              n_bytes_written == zip_file_bytes.size() );

    ::CloseHandle(file_handle);

end:
    return result;
}
