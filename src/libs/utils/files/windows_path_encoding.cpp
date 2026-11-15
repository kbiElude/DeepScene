/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "utils/files/windows_path_encoding.h"
#include <Windows.h>
#include <algorithm>

std::string Utils::Files::WindowsPathEncoding::to_utf8(const std::wstring& in_text)
{
    const int   n_utf8_bytes = ::WideCharToMultiByte(CP_UTF8,
                                                     0, /* flags */
                                                     in_text.c_str(),
                                                     static_cast<int>(in_text.size() ),
                                                     nullptr,
                                                     0,
                                                     nullptr,
                                                     nullptr);
    std::string result(static_cast<size_t>(n_utf8_bytes),
                       '\0');

    if (n_utf8_bytes > 0)
    {
        ::WideCharToMultiByte(CP_UTF8,
                              0, /* flags */
                              in_text.c_str(),
                              static_cast<int>(in_text.size() ),
                              &result.at(0),
                              n_utf8_bytes,
                              nullptr,
                              nullptr);
    }

    return result;
}

std::wstring Utils::Files::WindowsPathEncoding::to_wide(const std::string& in_text)
{
    std::wstring result = to_wide_text(in_text);

    std::replace(result.begin(),
                 result.end  (),
                 L'/',
                 L'\\');

    /* A path on a drive gets the long-path prefix, which lifts the MAX_PATH length limit. */
    if (result.size() >= 3                                       &&
        result.at(1) == L':'                                     &&
        result.at(2) == L'\\'                                    &&
        result.size() > 240)
    {
        result = L"\\\\?\\" + result;
    }

    return result;
}

std::wstring Utils::Files::WindowsPathEncoding::to_wide_text(const std::string& in_text)
{
    const int    n_wide_characters = ::MultiByteToWideChar(CP_UTF8,
                                                           0, /* flags */
                                                           in_text.c_str(),
                                                           static_cast<int>(in_text.size() ),
                                                           nullptr,
                                                           0);
    std::wstring result(static_cast<size_t>(n_wide_characters),
                        L'\0');

    if (n_wide_characters > 0)
    {
        ::MultiByteToWideChar(CP_UTF8,
                              0, /* flags */
                              in_text.c_str(),
                              static_cast<int>(in_text.size() ),
                              &result.at(0),
                              n_wide_characters);
    }

    return result;
}
