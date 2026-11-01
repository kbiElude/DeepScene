/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "VR/AppInstaller/file_system.h"
#include <Windows.h>
#include <algorithm>
#include <vector>

bool VR::AppInstaller::FileSystem::create_directories(const std::string& in_path,
                                                      std::string*       out_error_ptr)
{
    std::wstring path   = to_wide(in_path);
    bool         result = false;
    size_t       start  = 0;

    /* From the top: each directory which is not there is made. The prefix of a long path is not one of them. */
    start = (path.compare(0, 4, L"\\\\?\\") == 0) ? 4
                                                  : 0;

    while (start < path.size() )
    {
        const size_t       end      = path.find(L'\\',
                                                start);
        const size_t       stop     = (end == std::wstring::npos) ? path.size()
                                                                  : end;
        const std::wstring partial  = path.substr(0,
                                                  stop);
        const bool         is_drive = (partial.size() >= 2 && partial.at(partial.size() - 1) == L':');

        if (is_drive == false &&
            partial.size() > start)
        {
            const BOOL is_made = ::CreateDirectoryW(partial.c_str(),
                                                    nullptr);
            const DWORD error  = ::GetLastError();

            if (is_made == FALSE                &&
                error   != ERROR_ALREADY_EXISTS)
            {
                *out_error_ptr = "Cannot make the directory " + in_path + " (error " + std::to_string(error) + ")";

                goto end;
            }
        }

        start = stop + 1;
    }

    result = true;

end:
    return result;
}

bool VR::AppInstaller::FileSystem::is_directory(const std::string& in_path)
{
    const DWORD attributes = ::GetFileAttributesW(to_wide(in_path).c_str() );

    return (attributes != INVALID_FILE_ATTRIBUTES             &&
            (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0);
}

bool VR::AppInstaller::FileSystem::is_directory_empty(const std::string& in_path)
{
    WIN32_FIND_DATAW data;
    const HANDLE     find_handle = ::FindFirstFileW((to_wide(in_path) + L"\\*").c_str(),
                                                   &data);
    bool             result      = true;

    if (find_handle != INVALID_HANDLE_VALUE)
    {
        do
        {
            const std::wstring name(data.cFileName);

            if (name != L"." &&
                name != L"..")
            {
                result = false;

                break;
            }
        }
        while (::FindNextFileW(find_handle,
                              &data) != FALSE);

        ::FindClose(find_handle);
    }

    return result;
}

bool VR::AppInstaller::FileSystem::is_file(const std::string& in_path)
{
    const DWORD attributes = ::GetFileAttributesW(to_wide(in_path).c_str() );

    return (attributes != INVALID_FILE_ATTRIBUTES             &&
            (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0);
}

bool VR::AppInstaller::FileSystem::remove_tree(const std::string& in_path)
{
    const std::wstring path   = to_wide(in_path);
    bool               result = true;

    if (is_directory(in_path) )
    {
        WIN32_FIND_DATAW data;
        const HANDLE     find_handle = ::FindFirstFileW((path + L"\\*").c_str(),
                                                       &data);

        if (find_handle != INVALID_HANDLE_VALUE)
        {
            do
            {
                const std::wstring name(data.cFileName);

                if (name != L"." &&
                    name != L"..")
                {
                    const std::string child = in_path + "\\" + to_utf8(name);

                    if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
                    {
                        result = remove_tree(child) && result;
                    }
                    else
                    {
                        ::SetFileAttributesW(to_wide(child).c_str(),
                                             FILE_ATTRIBUTE_NORMAL);

                        result = (::DeleteFileW(to_wide(child).c_str() ) != FALSE) && result;
                    }
                }
            }
            while (::FindNextFileW(find_handle,
                                  &data) != FALSE);

            ::FindClose(find_handle);
        }

        result = (::RemoveDirectoryW(path.c_str() ) != FALSE) && result;
    }

    return result;
}

bool VR::AppInstaller::FileSystem::test_write(const std::string& in_directory,
                                              std::string*       out_error_ptr)
{
    const std::wstring path   = to_wide(in_directory + "\\write_test.tmp");
    HANDLE             handle = INVALID_HANDLE_VALUE;
    bool               result = false;

    handle = ::CreateFileW(path.c_str(),
                           GENERIC_WRITE,
                           0, /* share mode */
                           nullptr,
                           CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL | FILE_FLAG_DELETE_ON_CLOSE,
                           nullptr);

    if (handle == INVALID_HANDLE_VALUE)
    {
        *out_error_ptr = "Nothing can be written to " + in_directory + " (error " + std::to_string(::GetLastError() ) + ")";

        goto end;
    }

    ::CloseHandle(handle);

    result = true;

end:
    return result;
}

std::string VR::AppInstaller::FileSystem::to_utf8(const std::wstring& in_text)
{
    const int   n_bytes = ::WideCharToMultiByte(CP_UTF8,
                                                0, /* flags */
                                                in_text.c_str(),
                                                static_cast<int>(in_text.size() ),
                                                nullptr,
                                                0,
                                                nullptr,
                                                nullptr);
    std::string result(static_cast<size_t>(n_bytes),
                       '\0');

    if (n_bytes > 0)
    {
        ::WideCharToMultiByte(CP_UTF8,
                              0, /* flags */
                              in_text.c_str(),
                              static_cast<int>(in_text.size() ),
                              &result.at(0),
                              n_bytes,
                              nullptr,
                              nullptr);
    }

    return result;
}

std::wstring VR::AppInstaller::FileSystem::to_wide(const std::string& in_text)
{
    std::wstring result = to_wide_text(in_text);

    std::replace(result.begin(),
                 result.end  (),
                 L'/',
                 L'\\');

    /* A drive path gets the prefix, which lifts the limit of the length of a path. */
    if (result.size() >= 3                                       &&
        result.at(1) == L':'                                     &&
        result.at(2) == L'\\'                                    &&
        result.size() > 240)
    {
        result = L"\\\\?\\" + result;
    }

    return result;
}

std::wstring VR::AppInstaller::FileSystem::to_wide_text(const std::string& in_text)
{
    const int    n_characters = ::MultiByteToWideChar(CP_UTF8,
                                                      0, /* flags */
                                                      in_text.c_str(),
                                                      static_cast<int>(in_text.size() ),
                                                      nullptr,
                                                      0);
    std::wstring result(static_cast<size_t>(n_characters),
                        L'\0');

    if (n_characters > 0)
    {
        ::MultiByteToWideChar(CP_UTF8,
                              0, /* flags */
                              in_text.c_str(),
                              static_cast<int>(in_text.size() ),
                              &result.at(0),
                              n_characters);
    }

    return result;
}
