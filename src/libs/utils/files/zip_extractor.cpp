/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "utils/files/file_system.h"
#include "utils/files/zip_extractor.h"
#include "miniz.h"
#include <Windows.h>
#include <algorithm>
#include <set>
#include <stdio.h>
#include <vector>

bool Utils::Files::ZipExtractor::extract(const std::string&       in_archive_path,
                                         const std::string&       in_directory,
                                         const uint64_t&          in_n_unpacked_bytes_max,
                                         const std::atomic<bool>& in_is_cancelled,
                                         const ProgressCallback&  in_progress_func,
                                         std::string*             out_error_ptr)
{
    FILE*                 archive_file_ptr          = nullptr;
    bool                  is_zip_reader_open        = false;
    std::set<std::string> lower_case_entry_name_set;
    mz_uint               n_archive_entries         = 0;
    uint64_t              n_bytes_total             = 0;
    uint64_t              n_bytes_total_done        = 0;
    bool                  result                    = false;
    mz_zip_archive        zip_archive;

    mz_zip_zero_struct(&zip_archive);

    {
        const std::wstring wide_archive_path = FileSystem::to_wide(in_archive_path);

        archive_file_ptr = ::_wfopen(wide_archive_path.c_str(),
                                     L"rb");
    }

    if (archive_file_ptr == nullptr)
    {
        *out_error_ptr = "Cannot open " + in_archive_path;

        goto end;
    }

    if (mz_zip_reader_init_cfile(&zip_archive,
                                 archive_file_ptr,
                                 0, /* archive_size: from the file */
                                 0) == MZ_FALSE)
    {
        *out_error_ptr = std::string            ("Not a zip file: ") +
                         mz_zip_get_error_string(mz_zip_get_last_error(&zip_archive) );

        goto end;
    }

    is_zip_reader_open = true;
    n_archive_entries   = mz_zip_reader_get_num_files(&zip_archive);

    /* Everything is looked at before anything is written. */
    for (mz_uint n_entry = 0;
                 n_entry < n_archive_entries;
               ++n_entry)
    {
        mz_zip_archive_file_stat entry_stat;
        std::string              lower_case_entry_name;

        if (mz_zip_reader_file_stat(&zip_archive,
                                     n_entry,
                                    &entry_stat) == MZ_FALSE)
        {
            *out_error_ptr = "The zip file is damaged";

            goto end;
        }

        if (is_name_safe(entry_stat.m_filename) == false)
        {
            *out_error_ptr = std::string("The zip file has a name which is not allowed: ") +
                             entry_stat.m_filename;

            goto end;
        }

        if (entry_stat.m_is_encrypted != 0 ||
            entry_stat.m_is_supported == 0)
        {
            *out_error_ptr = std::string("The zip file has an entry which cannot be unpacked: ") +
                             entry_stat.m_filename;

            goto end;
        }

        lower_case_entry_name = to_lower(entry_stat.m_filename);

        if (lower_case_entry_name_set.insert(lower_case_entry_name).second == false)
        {
            *out_error_ptr = std::string("The zip file has a name twice: ") +
                             entry_stat.m_filename;

            goto end;
        }

        n_bytes_total += entry_stat.m_uncomp_size;

        if (n_bytes_total > in_n_unpacked_bytes_max)
        {
            *out_error_ptr = "The zip file holds more than it is allowed to";

            goto end;
        }
    }

    if (FileSystem::create_directories(in_directory,
                                       out_error_ptr) == false)
    {
        goto end;
    }

    for (mz_uint n_entry = 0;
                 n_entry < n_archive_entries;
               ++n_entry)
    {
        std::string              entry_path;
        mz_zip_archive_file_stat entry_stat;

        mz_zip_reader_file_stat(&zip_archive,
                                n_entry,
                               &entry_stat);

        entry_path = in_directory + "\\" + entry_stat.m_filename;

        std::replace(entry_path.begin(),
                     entry_path.end  (),
                     '/',
                     '\\');

        if (entry_stat.m_is_directory != 0)
        {
            if (FileSystem::create_directories(entry_path,
                                               out_error_ptr) == false)
            {
                goto end;
            }
        }
        else
        {
            HANDLE       file_handle                = INVALID_HANDLE_VALUE;
            mz_bool      is_entry_extracted         = MZ_FALSE;
            const size_t n_last_separator_character = entry_path.find_last_of('\\');
            WriteContext write_context;

            {
                const std::string entry_directory = entry_path.substr(0,
                                                                      n_last_separator_character);

                if (FileSystem::create_directories(entry_directory,
                                                   out_error_ptr) == false)
                {
                    goto end;
                }
            }

            {
                const std::wstring wide_entry_path = FileSystem::to_wide(entry_path);

                file_handle = ::CreateFileW(wide_entry_path.c_str(),
                                            GENERIC_WRITE,
                                            0, /* share mode */
                                            nullptr,
                                            CREATE_NEW,
                                            FILE_ATTRIBUTE_NORMAL,
                                            nullptr);
            }

            if (file_handle == INVALID_HANDLE_VALUE)
            {
                *out_error_ptr = "Cannot write "                   +
                                 entry_path                        +
                                 " (error "                        +
                                 std::to_string(::GetLastError() ) +
                                 ")";

                goto end;
            }

            write_context.file_handle_ptr    = file_handle;
            write_context.is_cancelled_ptr   = &in_is_cancelled;
            write_context.n_bytes_declared   = entry_stat.m_uncomp_size;
            write_context.n_bytes_total      = n_bytes_total;
            write_context.n_bytes_total_done = n_bytes_total_done;
            write_context.n_bytes_written    = 0;
            write_context.progress_func_ptr  = &in_progress_func;

            is_entry_extracted = mz_zip_reader_extract_to_callback(&zip_archive,
                                                                   n_entry,
                                                                  &write,
                                                                  &write_context,
                                                                   0); /* flags */

            ::CloseHandle(file_handle);

            if (in_is_cancelled.load() )
            {
                *out_error_ptr = "Cancelled";

                goto end;
            }

            if (is_entry_extracted            == MZ_FALSE                       ||
                write_context.n_bytes_written != write_context.n_bytes_declared)
            {
                *out_error_ptr = std::string("Cannot unpack ")                               +
                                 entry_stat.m_filename                                       +
                                 ": "                                                        +
                                 mz_zip_get_error_string(mz_zip_get_last_error(&zip_archive) );

                goto end;
            }

            n_bytes_total_done += write_context.n_bytes_written;
        }
    }

    result = true;

end:
    if (is_zip_reader_open)
    {
        mz_zip_reader_end(&zip_archive);
    }

    if (archive_file_ptr != nullptr)
    {
        ::fclose(archive_file_ptr);
    }

    return result;
}

bool Utils::Files::ZipExtractor::is_name_safe(const std::string& in_name)
{
    static const std::string DEVICE_NAME_ARRAY[] =
    {
        "con",
        "prn",
        "aux",
        "nul",
        "com1",
        "com2",
        "com3",
        "com4",
        "com5",
        "com6",
        "com7",
        "com8",
        "com9",
        "lpt1",
        "lpt2",
        "lpt3",
        "lpt4",
        "lpt5",
        "lpt6",
        "lpt7",
        "lpt8",
        "lpt9"
    };

    size_t      n_part_start_character    = 0;
    std::string name_with_forward_slashes = in_name;
    bool        result                    = false;

    std::replace(name_with_forward_slashes.begin(),
                 name_with_forward_slashes.end  (),
                 '\\',
                 '/');

    if (name_with_forward_slashes.empty()                          ||
        name_with_forward_slashes.at   (0)    == '/'               ||
        name_with_forward_slashes.find (':')  != std::string::npos ||
        name_with_forward_slashes.find ('\0') != std::string::npos)
    {
        goto end;
    }

    /* Each part of the name: not empty (but for the end of a directory), not "." or "..", not a device, and not ending with a dot or a space,
     * which the system does not keep.
     */
    while (n_part_start_character < name_with_forward_slashes.size() )
    {
        const size_t n_separator_character = name_with_forward_slashes.find('/',
                                                                            n_part_start_character);

        const size_t n_part_stop_character = (n_separator_character == std::string::npos) ? name_with_forward_slashes.size()
                                                                                          : n_separator_character;
        std::string  name_part             = name_with_forward_slashes.substr(n_part_start_character,
                                                                              n_part_stop_character - n_part_start_character);

        const size_t n_first_dot_character = name_part.find('.');
        std::string  name_part_before_dot  = (n_first_dot_character == std::string::npos) ? name_part
                                                                                          : name_part.substr(0,
                                                                                                             n_first_dot_character);

        if (name_part.empty()                          &&
            n_separator_character != std::string::npos)
        {
            goto end;
        }

        if (name_part          == "."                                   ||
            name_part          == ".."                                  ||
            (name_part.empty() == false && (name_part.back() == '.'  ||
                                            name_part.back() == ' ') ) )
        {
            goto end;
        }

        name_part_before_dot = to_lower(name_part_before_dot);

        for (const std::string& device_name : DEVICE_NAME_ARRAY)
        {
            if (name_part_before_dot == device_name)
            {
                goto end;
            }
        }

        n_part_start_character = n_part_stop_character + 1;
    }

    result = true;

end:
    return result;
}

std::string Utils::Files::ZipExtractor::to_lower(const std::string& in_text)
{
    std::string result = in_text;

    const auto to_lower_func = [](const char in_character)
    {
        return static_cast<char>(::tolower(static_cast<unsigned char>(in_character) ) );
    };

    std::transform(result.begin(),
                   result.end  (),
                   result.begin(),
                   to_lower_func);

    return result;
}

size_t Utils::Files::ZipExtractor::write(void*       in_context_ptr,
                                         uint64_t    in_offset,
                                         const void* in_bytes_ptr,
                                         size_t      in_n_bytes)
{
    WriteContext* context_ptr             = static_cast<WriteContext*>(in_context_ptr);
    DWORD         n_bytes_written_by_call = 0;
    size_t        result                  = 0;

    /* An entry is not written beyond its size, and not at all once it is cancelled; the caller sees the difference. */
    if (context_ptr->is_cancelled_ptr->load()                                      ||
        context_ptr->n_bytes_written + in_n_bytes >  context_ptr->n_bytes_declared ||
        in_offset                                 != context_ptr->n_bytes_written)
    {
        goto end;
    }

    if (::WriteFile(static_cast<HANDLE>(context_ptr->file_handle_ptr),
                    in_bytes_ptr,
                    static_cast<DWORD>(in_n_bytes),
                   &n_bytes_written_by_call,
                    nullptr) == FALSE)
    {
        goto end;
    }

    context_ptr->n_bytes_written += n_bytes_written_by_call;
    result                        = n_bytes_written_by_call;

    if (*context_ptr->progress_func_ptr)
    {
        (*context_ptr->progress_func_ptr)(context_ptr->n_bytes_total_done + context_ptr->n_bytes_written,
                                          context_ptr->n_bytes_total);
    }

end:
    return result;
}
