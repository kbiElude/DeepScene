/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "VR/AppInstaller/file_system.h"
#include "VR/AppInstaller/zip_extractor.h"
#include "miniz.h"
#include <Windows.h>
#include <algorithm>
#include <set>
#include <stdio.h>
#include <vector>

bool VR::AppInstaller::ZipExtractor::extract(const std::string&       in_archive_path,
                                             const std::string&       in_directory,
                                             const uint64_t&          in_n_unpacked_bytes_max,
                                             const std::atomic<bool>& in_is_cancelled,
                                             const ProgressCallback&  in_progress_func,
                                             std::string*             out_error_ptr)
{
    FILE*                 file_ptr          = nullptr;
    std::set<std::string> lower_name_set;
    mz_uint               n_entries         = 0;
    uint64_t              n_bytes_total     = 0;
    uint64_t              n_bytes_total_done = 0;
    bool                  result            = false;
    bool                  is_zip_open       = false;
    mz_zip_archive        zip;

    mz_zip_zero_struct(&zip);

    file_ptr = ::_wfopen(FileSystem::to_wide(in_archive_path).c_str(),
                         L"rb");

    if (file_ptr == nullptr)
    {
        *out_error_ptr = "Cannot open " + in_archive_path;

        goto end;
    }

    if (mz_zip_reader_init_cfile(&zip,
                                 file_ptr,
                                 0, /* archive_size: from the file */
                                 0) == MZ_FALSE)
    {
        *out_error_ptr = std::string("Not a zip file: ") + mz_zip_get_error_string(mz_zip_get_last_error(&zip) );

        goto end;
    }

    is_zip_open = true;
    n_entries   = mz_zip_reader_get_num_files(&zip);

    /* Everything is looked at before anything is written. */
    for (mz_uint n_entry = 0;
                 n_entry < n_entries;
               ++n_entry)
    {
        mz_zip_archive_file_stat stat;
        std::string              lower_name;

        if (mz_zip_reader_file_stat(&zip,
                                    n_entry,
                                   &stat) == MZ_FALSE)
        {
            *out_error_ptr = "The zip file is damaged";

            goto end;
        }

        if (is_name_safe(stat.m_filename) == false)
        {
            *out_error_ptr = std::string("The zip file has a name which is not allowed: ") + stat.m_filename;

            goto end;
        }

        if (stat.m_is_encrypted != 0 ||
            stat.m_is_supported == 0)
        {
            *out_error_ptr = std::string("The zip file has an entry which cannot be unpacked: ") + stat.m_filename;

            goto end;
        }

        lower_name = to_lower(stat.m_filename);

        if (lower_name_set.insert(lower_name).second == false)
        {
            *out_error_ptr = std::string("The zip file has a name twice: ") + stat.m_filename;

            goto end;
        }

        n_bytes_total += stat.m_uncomp_size;

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
                 n_entry < n_entries;
               ++n_entry)
    {
        mz_zip_archive_file_stat stat;
        std::string              path;

        mz_zip_reader_file_stat(&zip,
                                n_entry,
                               &stat);

        path = in_directory + "\\" + stat.m_filename;

        std::replace(path.begin(),
                     path.end  (),
                     '/',
                     '\\');

        if (stat.m_is_directory != 0)
        {
            if (FileSystem::create_directories(path,
                                               out_error_ptr) == false)
            {
                goto end;
            }
        }
        else
        {
            const size_t n_separator = path.find_last_of('\\');
            WriteContext context;
            HANDLE       file_handle = INVALID_HANDLE_VALUE;
            mz_bool      is_extracted = MZ_FALSE;

            if (FileSystem::create_directories(path.substr(0,
                                                           n_separator),
                                               out_error_ptr) == false)
            {
                goto end;
            }

            file_handle = ::CreateFileW(FileSystem::to_wide(path).c_str(),
                                        GENERIC_WRITE,
                                        0, /* share mode */
                                        nullptr,
                                        CREATE_NEW,
                                        FILE_ATTRIBUTE_NORMAL,
                                        nullptr);

            if (file_handle == INVALID_HANDLE_VALUE)
            {
                *out_error_ptr = "Cannot write " + path + " (error " + std::to_string(::GetLastError() ) + ")";

                goto end;
            }

            context.file_handle        = file_handle;
            context.is_cancelled_ptr   = &in_is_cancelled;
            context.n_bytes_declared   = stat.m_uncomp_size;
            context.n_bytes_total      = n_bytes_total;
            context.n_bytes_total_done = n_bytes_total_done;
            context.n_bytes_written    = 0;
            context.progress_func_ptr  = &in_progress_func;

            is_extracted = mz_zip_reader_extract_to_callback(&zip,
                                                             n_entry,
                                                             &write,
                                                             &context,
                                                             0); /* flags */

            ::CloseHandle(file_handle);

            if (in_is_cancelled.load() )
            {
                *out_error_ptr = "Cancelled";

                goto end;
            }

            if (is_extracted == MZ_FALSE                          ||
                context.n_bytes_written != context.n_bytes_declared)
            {
                *out_error_ptr = std::string("Cannot unpack ") + stat.m_filename + ": " +
                                 mz_zip_get_error_string(mz_zip_get_last_error(&zip) );

                goto end;
            }

            n_bytes_total_done += context.n_bytes_written;
        }
    }

    result = true;

end:
    if (is_zip_open)
    {
        mz_zip_reader_end(&zip);
    }

    if (file_ptr != nullptr)
    {
        ::fclose(file_ptr);
    }

    return result;
}

bool VR::AppInstaller::ZipExtractor::is_name_safe(const std::string& in_name)
{
    static const char* const DEVICE_NAME_ARRAY[] = {"con", "prn", "aux", "nul", "com1", "com2", "com3", "com4", "com5", "com6", "com7", "com8",
                                                    "com9", "lpt1", "lpt2", "lpt3", "lpt4", "lpt5", "lpt6", "lpt7", "lpt8", "lpt9"};

    std::string normalized = in_name;
    bool        result     = false;
    size_t      start      = 0;

    std::replace(normalized.begin(),
                 normalized.end  (),
                 '\\',
                 '/');

    if (normalized.empty()                                         ||
        normalized.at(0) == '/'                                    ||
        normalized.find(':')  != std::string::npos                 ||
        normalized.find('\0') != std::string::npos)
    {
        goto end;
    }

    /* Each part of the name: not empty (but for the end of a directory), not "." or "..", not a device, and not ending with a dot or a space,
     * which the system does not keep.
     */
    while (start < normalized.size() )
    {
        const size_t      end  = normalized.find('/',
                                                 start);
        const size_t      stop = (end == std::string::npos) ? normalized.size()
                                                            : end;
        std::string       part = normalized.substr(start,
                                                   stop - start);
        const size_t      n_dot = part.find('.');
        std::string       base  = (n_dot == std::string::npos) ? part
                                                               : part.substr(0,
                                                                             n_dot);

        if (part.empty() && end != std::string::npos)
        {
            goto end;
        }

        if (part == "."  ||
            part == ".." ||
            (part.empty() == false && (part.back() == '.' || part.back() == ' ') ) )
        {
            goto end;
        }

        base = to_lower(base);

        for (const char* device_ptr : DEVICE_NAME_ARRAY)
        {
            if (base == device_ptr)
            {
                goto end;
            }
        }

        start = stop + 1;
    }

    result = true;

end:
    return result;
}

std::string VR::AppInstaller::ZipExtractor::to_lower(const std::string& in_text)
{
    std::string result = in_text;

    std::transform(result.begin(),
                   result.end  (),
                   result.begin(),
                   [](const char in_character)
                   {
                       return static_cast<char>(::tolower(static_cast<unsigned char>(in_character) ) );
                   });

    return result;
}

size_t VR::AppInstaller::ZipExtractor::write(void*       in_context_ptr,
                                             uint64_t    in_offset,
                                             const void* in_bytes_ptr,
                                             size_t      in_n_bytes)
{
    WriteContext* context_ptr = static_cast<WriteContext*>(in_context_ptr);
    DWORD         n_written   = 0;
    size_t        result      = 0;

    /* An entry is not written beyond its size, and not at all once it is cancelled; the caller sees the difference. */
    if (context_ptr->is_cancelled_ptr->load()                                                      ||
        context_ptr->n_bytes_written + in_n_bytes > context_ptr->n_bytes_declared                  ||
        in_offset != context_ptr->n_bytes_written)
    {
        goto end;
    }

    if (::WriteFile(static_cast<HANDLE>(context_ptr->file_handle),
                    in_bytes_ptr,
                    static_cast<DWORD>(in_n_bytes),
                   &n_written,
                    nullptr) == FALSE)
    {
        goto end;
    }

    context_ptr->n_bytes_written += n_written;
    result                        = n_written;

    if (*context_ptr->progress_func_ptr)
    {
        (*context_ptr->progress_func_ptr)(context_ptr->n_bytes_total_done + context_ptr->n_bytes_written,
                                          context_ptr->n_bytes_total);
    }

end:
    return result;
}
