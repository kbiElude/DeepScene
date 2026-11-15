/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "utils/files/directory_operations.h"
#include "utils/files/windows_path_encoding.h"
#include <Windows.h>
#include <vector>

bool Utils::Files::DirectoryOperations::create_directories(const std::string& in_path,
                                                           std::string*       out_error_ptr)
{
    size_t       n_segment_start_character = 0;
    bool         result                    = false;
    std::wstring wide_path                 = Utils::Files::WindowsPathEncoding::to_wide(in_path);

    /* Creates each missing directory from the top down. The long-path prefix is not treated as a directory. */
    n_segment_start_character = (wide_path.compare(0, 4, L"\\\\?\\") == 0) ? 4
                                                                           : 0;

    while (n_segment_start_character < wide_path.size() )
    {
        const size_t n_separator_character = wide_path.find(L'\\',
                                                            n_segment_start_character);

        const size_t n_segment_stop_character = (n_separator_character == std::wstring::npos) ? wide_path.size()
                                                                                              : n_separator_character;

        const std::wstring directory_path_so_far = wide_path.substr(0,
                                                                    n_segment_stop_character);
        const bool         is_drive_root         = (directory_path_so_far.size()                                 >= 2    &&
                                                    directory_path_so_far.at  (directory_path_so_far.size() - 1) == L':');

        if (is_drive_root == false &&
            directory_path_so_far.size() > n_segment_start_character)
        {
            const BOOL is_made = ::CreateDirectoryW(directory_path_so_far.c_str(),
                                                    nullptr);

            const DWORD error = ::GetLastError();

            if (is_made == FALSE                &&
                error   != ERROR_ALREADY_EXISTS)
            {
                *out_error_ptr = "Cannot make the directory " + in_path + " (error " + std::to_string(error) + ")";

                goto end;
            }
        }

        n_segment_start_character = n_segment_stop_character + 1;
    }

    result = true;

end:
    return result;
}

bool Utils::Files::DirectoryOperations::is_directory(const std::string& in_path)
{
    const DWORD file_attributes = ::GetFileAttributesW(Utils::Files::WindowsPathEncoding::to_wide(in_path).c_str() );

    return (file_attributes                              != INVALID_FILE_ATTRIBUTES &&
            (file_attributes & FILE_ATTRIBUTE_DIRECTORY) != 0);
}

bool Utils::Files::DirectoryOperations::is_directory_empty(const std::string& in_path)
{
    WIN32_FIND_DATAW   find_data;
    const std::wstring wide_pattern = Utils::Files::WindowsPathEncoding::to_wide(in_path) + L"\\*";
    const HANDLE       find_handle  = ::FindFirstFileW(wide_pattern.c_str(),
                                                       &find_data);
    bool               result       = true;

    if (find_handle != INVALID_HANDLE_VALUE)
    {
        do
        {
            const std::wstring entry_name(find_data.cFileName);

            if (entry_name != L"." &&
                entry_name != L"..")
            {
                result = false;

                break;
            }
        }
        while (::FindNextFileW(find_handle,
                              &find_data) != FALSE);

        ::FindClose(find_handle);
    }

    return result;
}

bool Utils::Files::DirectoryOperations::is_file(const std::string& in_path)
{
    const DWORD file_attributes = ::GetFileAttributesW(Utils::Files::WindowsPathEncoding::to_wide(in_path).c_str() );

    return (file_attributes != INVALID_FILE_ATTRIBUTES &&
            (file_attributes & FILE_ATTRIBUTE_DIRECTORY) == 0);
}

bool Utils::Files::DirectoryOperations::remove_tree(const std::string& in_path)
{
    bool               result    = true;
    const std::wstring wide_path = Utils::Files::WindowsPathEncoding::to_wide(in_path);

    if (is_directory(in_path) )
    {
        WIN32_FIND_DATAW find_data;
        const HANDLE     find_handle = ::FindFirstFileW((wide_path + L"\\*").c_str(),
                                                       &find_data);

        if (find_handle != INVALID_HANDLE_VALUE)
        {
            do
            {
                const std::wstring entry_name(find_data.cFileName);

                if (entry_name != L"." &&
                    entry_name != L"..")
                {
                    const std::string child_path = in_path + "\\" + Utils::Files::WindowsPathEncoding::to_utf8(entry_name);

                    if ((find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
                    {
                        const bool is_child_removed = remove_tree(child_path);

                        result = (is_child_removed && result);
                    }
                    else
                    {
                        const std::wstring wide_child_path = Utils::Files::WindowsPathEncoding::to_wide(child_path);

                        ::SetFileAttributesW(wide_child_path.c_str(),
                                             FILE_ATTRIBUTE_NORMAL);

                        const BOOL is_file_deleted = ::DeleteFileW(wide_child_path.c_str() );

                        result = (is_file_deleted != FALSE &&
                                  result);
                    }
                }
            }
            while (::FindNextFileW(find_handle,
                                  &find_data) != FALSE);

            ::FindClose(find_handle);
        }

        const BOOL is_directory_removed = ::RemoveDirectoryW(wide_path.c_str() );

        result = (is_directory_removed != FALSE &&
                  result);
    }

    return result;
}

bool Utils::Files::DirectoryOperations::test_write(const std::string& in_directory,
                                                   std::string*       out_error_ptr)
{
    bool               result           = false;
    HANDLE             test_file_handle = INVALID_HANDLE_VALUE;
    const std::wstring test_file_path   = Utils::Files::WindowsPathEncoding::to_wide(in_directory + "\\write_test.tmp");

    test_file_handle = ::CreateFileW(test_file_path.c_str(),
                                     GENERIC_WRITE,
                                     0, /* share mode */
                                     nullptr,
                                     CREATE_ALWAYS,
                                     FILE_ATTRIBUTE_NORMAL | FILE_FLAG_DELETE_ON_CLOSE,
                                     nullptr);

    if (test_file_handle == INVALID_HANDLE_VALUE)
    {
        *out_error_ptr = "Nothing can be written to " + in_directory + " (error " + std::to_string(::GetLastError() ) + ")";

        goto end;
    }

    ::CloseHandle(test_file_handle);

    result = true;

end:
    return result;
}
