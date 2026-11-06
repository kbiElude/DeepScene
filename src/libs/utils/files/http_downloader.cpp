/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "utils/files/file_system.h"
#include "utils/files/http_downloader.h"
#include <Windows.h>
#include <winhttp.h>
#include <algorithm>
#include <stdint.h>
#include <vector>

Utils::Files::HttpDownloader::HttpDownloader(const bool& in_is_http_allowed,
                                             const int&  in_receive_timeout_ms)
    :m_is_http_allowed   (in_is_http_allowed),
     m_receive_timeout_ms(in_receive_timeout_ms)
{
    /* Nothing to do. */
}

bool Utils::Files::HttpDownloader::download(const std::string&       in_url,
                                            const std::string&       in_file_path,
                                            const std::atomic<bool>& in_is_cancelled,
                                            const ProgressCallback&  in_progress_func,
                                            std::string*             out_error_ptr)
{
    constexpr DWORD CHUNK_N_BYTES      = 64 * 1024;
    constexpr int   CONNECT_TIMEOUT_MS = 15000;
    constexpr int   N_MAX_REDIRECTS    = 10;
    constexpr int   RESOLVE_TIMEOUT_MS = 10000;
    constexpr int   SEND_TIMEOUT_MS    = 15000;

    HINTERNET    connection_handle      = nullptr;
    HANDLE       file_handle            = INVALID_HANDLE_VALUE;
    std::wstring host_name;
    DWORD        http_status_code       = 0;
    DWORD        n_available_bytes      = 0;
    uint64_t     n_bytes_done           = 0;
    uint64_t     n_bytes_total          = 0;
    DWORD        n_content_length_bytes = 0;
    DWORD        n_header_value_bytes   = sizeof(DWORD);
    DWORD        n_read_bytes           = 0;
    std::vector<uint8_t> read_buffer_vec(CHUNK_N_BYTES);
    HINTERNET          request_handle          = nullptr;
    std::wstring       resource_path_and_query;
    bool               result                  = false;
    HINTERNET          session_handle          = nullptr;
    URL_COMPONENTSW    url_components;
    const std::wstring wide_url                = FileSystem::to_wide_text(in_url);

    ::ZeroMemory(&url_components,
                 sizeof(url_components) );

    url_components.dwStructSize     = sizeof(url_components);
    url_components.dwHostNameLength = static_cast<DWORD>(-1);
    url_components.dwSchemeLength   = static_cast<DWORD>(-1);
    url_components.dwUrlPathLength  = static_cast<DWORD>(-1);
    url_components.dwExtraInfoLength = static_cast<DWORD>(-1);

    if (::WinHttpCrackUrl(wide_url.c_str(),
                          static_cast<DWORD>(wide_url.size() ),
                          0, /* flags */
                         &url_components) == FALSE)
    {
        *out_error_ptr = "Not an address: " + in_url;

        goto end;
    }

    if (url_components.nScheme != INTERNET_SCHEME_HTTPS &&
        (url_components.nScheme != INTERNET_SCHEME_HTTP || m_is_http_allowed == false) )
    {
        *out_error_ptr = "Only addresses of https are used: " + in_url;

        goto end;
    }

    host_name     = std::wstring(url_components.lpszHostName,
                                 url_components.dwHostNameLength);
    resource_path_and_query = std::wstring(url_components.lpszUrlPath,
                                           url_components.dwUrlPathLength + url_components.dwExtraInfoLength);

    session_handle = ::WinHttpOpen(L"DeepScene/1.0",
                                   WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                   WINHTTP_NO_PROXY_NAME,
                                   WINHTTP_NO_PROXY_BYPASS,
                                   0);

    if (session_handle == nullptr)
    {
        *out_error_ptr = "The network of the system cannot be used (error " + std::to_string(::GetLastError() ) + ")";

        goto end;
    }

    ::WinHttpSetTimeouts(session_handle,
                         RESOLVE_TIMEOUT_MS,
                         CONNECT_TIMEOUT_MS,
                         SEND_TIMEOUT_MS,
                         m_receive_timeout_ms);

    {
        DWORD n_redirects_max = N_MAX_REDIRECTS;

        ::WinHttpSetOption(session_handle,
                           WINHTTP_OPTION_MAX_HTTP_AUTOMATIC_REDIRECTS,
                          &n_redirects_max,
                           sizeof(n_redirects_max) );
    }

    connection_handle = ::WinHttpConnect(session_handle,
                                         host_name.c_str(),
                                         url_components.nPort,
                                         0);

    if (connection_handle == nullptr)
    {
        *out_error_ptr = "Cannot reach " + FileSystem::to_utf8(host_name) + " (error " + std::to_string(::GetLastError() ) + ")";

        goto end;
    }

    request_handle = ::WinHttpOpenRequest(connection_handle,
                                          L"GET",
                                          resource_path_and_query.c_str(),
                                          nullptr,
                                          WINHTTP_NO_REFERER,
                                          WINHTTP_DEFAULT_ACCEPT_TYPES,
                                          (url_components.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE
                                                                                            : 0);

    if (request_handle == nullptr)
    {
        *out_error_ptr = "Cannot ask " + FileSystem::to_utf8(host_name) + " (error " + std::to_string(::GetLastError() ) + ")";

        goto end;
    }

    if (::WinHttpSendRequest(request_handle,
                             WINHTTP_NO_ADDITIONAL_HEADERS,
                             0,
                             WINHTTP_NO_REQUEST_DATA,
                             0,
                             0,
                             0) == FALSE)
    {
        *out_error_ptr = "Cannot ask " + FileSystem::to_utf8(host_name) + " (error " + std::to_string(::GetLastError() ) + ")";

        goto end;
    }

    if (::WinHttpReceiveResponse(request_handle,
                                 nullptr) == FALSE)
    {
        *out_error_ptr = "No answer from " + FileSystem::to_utf8(host_name) + " (error " + std::to_string(::GetLastError() ) + ")";

        goto end;
    }

    ::WinHttpQueryHeaders(request_handle,
                          WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                          WINHTTP_HEADER_NAME_BY_INDEX,
                         &http_status_code,
                         &n_header_value_bytes,
                          WINHTTP_NO_HEADER_INDEX);

    if (http_status_code != 200)
    {
        *out_error_ptr = FileSystem::to_utf8(host_name) + " answered " + std::to_string(http_status_code);

        goto end;
    }

    n_header_value_bytes = sizeof(DWORD);

    if (::WinHttpQueryHeaders(request_handle,
                              WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER,
                              WINHTTP_HEADER_NAME_BY_INDEX,
                             &n_content_length_bytes,
                             &n_header_value_bytes,
                              WINHTTP_NO_HEADER_INDEX) != FALSE)
    {
        n_bytes_total = n_content_length_bytes;
    }

    {
        const std::wstring wide_file_path = FileSystem::to_wide(in_file_path);

        file_handle = ::CreateFileW(wide_file_path.c_str(),
                                    GENERIC_WRITE,
                                    0, /* share mode */
                                    nullptr,
                                    CREATE_ALWAYS,
                                    FILE_ATTRIBUTE_NORMAL,
                                    nullptr);
    }

    if (file_handle == INVALID_HANDLE_VALUE)
    {
        *out_error_ptr = "Cannot write " + in_file_path + " (error " + std::to_string(::GetLastError() ) + ")";

        goto end;
    }

    do
    {
        DWORD n_written_bytes = 0;

        if (in_is_cancelled.load() )
        {
            *out_error_ptr = "Cancelled";

            goto end;
        }

        n_available_bytes = 0;

        if (::WinHttpQueryDataAvailable(request_handle,
                                       &n_available_bytes) == FALSE)
        {
            *out_error_ptr = (in_is_cancelled.load() ) ? std::string("Cancelled")
                                                       : "The download stopped (error " + std::to_string(::GetLastError() ) + ")";

            goto end;
        }

        if (n_available_bytes == 0)
        {
            break;
        }

        if (::WinHttpReadData(request_handle,
                              &read_buffer_vec.at(0),
                              std::min<DWORD>(n_available_bytes,
                                              CHUNK_N_BYTES),
                             &n_read_bytes) == FALSE)
        {
            *out_error_ptr = "The download stopped (error " + std::to_string(::GetLastError() ) + ")";

            goto end;
        }

        const BOOL is_written = ::WriteFile(file_handle,
                                            &read_buffer_vec.at(0),
                                            n_read_bytes,
                                           &n_written_bytes,
                                            nullptr);

        if (is_written == FALSE              ||
            n_written_bytes != n_read_bytes)
        {
            *out_error_ptr = "Cannot write " + in_file_path + " (error " + std::to_string(::GetLastError() ) + ")";

            goto end;
        }

        n_bytes_done += n_read_bytes;

        if (in_progress_func)
        {
            in_progress_func(n_bytes_done,
                             n_bytes_total);
        }
    }
    while (true);

    if (n_bytes_total != 0       &&
        n_bytes_done  != n_bytes_total)
    {
        *out_error_ptr = "The download ended before the end of the file";

        goto end;
    }

    result = true;

end:
    if (file_handle != INVALID_HANDLE_VALUE)
    {
        ::CloseHandle(file_handle);
    }

    if (request_handle != nullptr)
    {
        ::WinHttpCloseHandle(request_handle);
    }

    if (connection_handle != nullptr)
    {
        ::WinHttpCloseHandle(connection_handle);
    }

    if (session_handle != nullptr)
    {
        ::WinHttpCloseHandle(session_handle);
    }

    return result;
}
