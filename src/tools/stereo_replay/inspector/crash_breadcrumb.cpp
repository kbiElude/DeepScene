/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "tools/stereo_replay/inspector/crash_breadcrumb.h"
#include "capture/api_dump_names/function_names.h"
#include "common/argument_reader.h"
#include <stdio.h>
#include <string.h>
#include <windows.h>

Stereo::Replay::Inspector::CrashBreadcrumb::CrashBreadcrumb(const std::string& in_filename)
    :m_file_handle   (nullptr),
     m_mapping_handle(nullptr),
     m_n_calls       (0),
     m_view_ptr      (nullptr)
{
    HANDLE file_handle = ::CreateFileA(in_filename.c_str(),
                                       GENERIC_READ | GENERIC_WRITE,
                                       FILE_SHARE_READ | FILE_SHARE_WRITE,
                                       nullptr,
                                       CREATE_ALWAYS,
                                       FILE_ATTRIBUTE_NORMAL,
                                       nullptr);

    if (file_handle != INVALID_HANDLE_VALUE)
    {
        HANDLE mapping_handle = ::CreateFileMappingA(file_handle,
                                                     nullptr,
                                                     PAGE_READWRITE,
                                                     0,
                                                     static_cast<DWORD>(LINE_N_BYTES),
                                                     nullptr);

        if (mapping_handle != nullptr)
        {
            void* view_ptr = ::MapViewOfFile(mapping_handle,
                                             FILE_MAP_WRITE,
                                             0,
                                             0,
                                             LINE_N_BYTES);

            if (view_ptr != nullptr)
            {
                m_file_handle    = file_handle;
                m_mapping_handle = mapping_handle;
                m_view_ptr       = static_cast<char*>(view_ptr);

                ::memset(m_view_ptr,
                         0,
                         LINE_N_BYTES);
            }
            else
            {
                ::CloseHandle(mapping_handle);
                ::CloseHandle(file_handle);
            }
        }
        else
        {
            ::CloseHandle(file_handle);
        }
    }
}

Stereo::Replay::Inspector::CrashBreadcrumb::~CrashBreadcrumb()
{
    if (m_view_ptr != nullptr)
    {
        ::UnmapViewOfFile(m_view_ptr);
        ::CloseHandle    (m_mapping_handle);
        ::CloseHandle    (m_file_handle);
    }
}

std::string Stereo::Replay::Inspector::CrashBreadcrumb::format_call(const std::string&                                    in_state,
                                                                    const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                    const uint32_t&                              in_n_frame,
                                                                    const uint64_t&                              in_n_call)
{
    std::string result;
    char        text[64];

    ::snprintf(text,
               sizeof(text),
               " frame=%u call=%llu thread=%u ",
               in_n_frame,
               static_cast<unsigned long long>(in_n_call),
               in_call.thread_id);

    result  = in_state;
    result += text;
    result += Capture::APIDumpNames::get_function_name(in_call.func);
    result += "(";

    for (size_t n_argument = 0;
                n_argument < in_call.arg_in_vec.size();
              ++n_argument)
    {
        const auto& argument = in_call.arg_in_vec.at(n_argument);
        double      number   = 0.0;

        if (n_argument > 0)
        {
            result += ", ";
        }

        if (argument.type == APIInterceptor::ARGTYPE_VOID_PTR)
        {
            ::snprintf(text,
                       sizeof(text),
                       "0x%llx",
                       static_cast<unsigned long long>(argument.get_ptr_as_u64() ) );

            result += text;
        }
        else
        if (argument.type == APIInterceptor::ARGTYPE_DATA_CHUNK_ID)
        {
            ::snprintf(text,
                       sizeof(text),
                       "chunk#%u",
                       argument.get_data_chunk_id() );

            result += text;
        }
        else
        if (APIInterceptor::read_number(argument,
                                        &number) )
        {
            ::snprintf(text,
                       sizeof(text),
                       "%g",
                       number);

            result += text;
        }
        else
        {
            result += "?";
        }
    }

    result += ")";

    return result;
}

bool Stereo::Replay::Inspector::CrashBreadcrumb::is_open() const
{
    return (m_view_ptr != nullptr);
}

void Stereo::Replay::Inspector::CrashBreadcrumb::on_call_replayed(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                  const uint32_t&                              in_n_frame,
                                                                  const bool&                                  in_was_executed)
{
    (void) in_was_executed;

    if (m_view_ptr != nullptr)
    {
        write_line(format_call("finished",
                               in_call,
                               in_n_frame,
                               m_n_calls) );
    }
}

bool Stereo::Replay::Inspector::CrashBreadcrumb::should_execute_call(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                     const uint32_t&                              in_n_frame)
{
    if (m_view_ptr != nullptr)
    {
        ++m_n_calls;

        write_line(format_call("begun",
                               in_call,
                               in_n_frame,
                               m_n_calls) );
    }

    return true;
}

void Stereo::Replay::Inspector::CrashBreadcrumb::write_line(const std::string& in_line)
{
    /* The new line is written completely before the rest of the previous line is cleared, so that a crash in the middle leaves a whole line. */
    const size_t n_bytes = (in_line.size() < LINE_N_BYTES - 1) ? in_line.size()
                                                               : LINE_N_BYTES - 1;

    ::memcpy(m_view_ptr,
             in_line.data(),
             n_bytes);

    m_view_ptr[n_bytes] = '\n';

    /* Clear what is left of a longer previous line. */
    ::memset(m_view_ptr + n_bytes + 1,
             0,
             LINE_N_BYTES - n_bytes - 1);
}
