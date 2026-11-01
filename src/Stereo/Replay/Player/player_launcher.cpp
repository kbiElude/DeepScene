/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "player_launcher.h"
#include "command_line.h"
#include "Capture/APIDumpLoader/WorkloadReader.h"
#include <stdlib.h>
#include <vector>
#include <Windows.h>

std::wstring Stereo::Replay::Player::PlayerLauncher::get_wide_string(const std::string& in_text)
{
    const int n_characters = ::MultiByteToWideChar(CP_ACP,
                                                   0, /* flags */
                                                   in_text.c_str(),
                                                   static_cast<int>(in_text.size() ),
                                                   nullptr,
                                                   0);

    std::wstring result(static_cast<size_t>(n_characters),
                        L'\0');

    if (n_characters > 0)
    {
        ::MultiByteToWideChar(CP_ACP,
                              0, /* flags */
                              in_text.c_str   (),
                              static_cast<int>(in_text.size() ),
                              &result.front   (),
                              n_characters);
    }

    return result;
}

bool Stereo::Replay::Player::PlayerLauncher::is_handover_needed(const char* in_workload_path_ptr)
{
    constexpr uint32_t N_BYTES_PER_POINTER_X86 = 4;

    /* Only a 64-bit player hands over, and only a dump with 32-bit pointers can hold D3D8. */
    bool result = false;

    if (sizeof(void*) != N_BYTES_PER_POINTER_X86)
    {
        const auto reader_ptr = Capture::APIDumpLoader::WorkloadReader::create(in_workload_path_ptr);

        if (reader_ptr != nullptr)
        {
            if (reader_ptr->get_pointer_n_bytes() == N_BYTES_PER_POINTER_X86)
            {
                result = reader_ptr->contains_api_group(Capture::APIDumpLoader::APIGroup::D3D8);
            }
        }
    }

    return result;
}

bool Stereo::Replay::Player::PlayerLauncher::run_x86_player(const Options& in_options,
                                                            int*           out_exit_code_ptr,
                                                            std::string*   out_error_ptr)
{
    constexpr const wchar_t* X86_PLAYER_FILE_NAME = L"StereoReplayPlayer_x86.exe";

    std::wstring                         command_line;
    DWORD                                exit_code           = EXIT_FAILURE;
    HANDLE                               job_handle          = nullptr;
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION job_limits          = {};
    PROCESS_INFORMATION                  process_information = {};
    bool                                 result              = false;
    STARTUPINFOW                         startup_information = {};
    std::wstring                         x86_player_path;

    std::vector<wchar_t> module_path_vec(MAX_PATH);

    startup_information.cb = sizeof(startup_information);

    if (::GetModuleFileNameW(nullptr,
                             module_path_vec.data(),
                             static_cast<DWORD>(module_path_vec.size() ) ) == 0)
    {
        *out_error_ptr = "The location of this player is unknown.";

        goto end;
    }

    /* The 32-bit player is in the folder of this one. */
    x86_player_path = module_path_vec.data  ();
    x86_player_path = x86_player_path.substr(0,
                                             x86_player_path.find_last_of(L'\\') + 1) + X86_PLAYER_FILE_NAME;

    if (::GetFileAttributesW(x86_player_path.c_str() ) == INVALID_FILE_ATTRIBUTES)
    {
        *out_error_ptr = "This workload holds D3D8 calls, which only the 32-bit player can replay, but StereoReplayPlayer_x86.exe is not next to this player.";

        goto end;
    }

    /* The child must not outlive this process, as it would go on using the console. */
    job_handle                                  = ::CreateJobObjectW(nullptr,
                                                                     nullptr);
    job_limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;

    if (job_handle != nullptr)
    {
        ::SetInformationJobObject(job_handle,
                                  JobObjectExtendedLimitInformation,
                                 &job_limits,
                                  sizeof(job_limits) );
    }

    command_line = L"\""           +
                   x86_player_path +
                   L"\"";

    for (const auto& argument : CommandLine::get_handover_argument_vec(in_options) )
    {
        command_line += L" \""                    +
                        get_wide_string(argument) +
                        L"\"";
    }

    command_line += L" \""                                    +
                    get_wide_string(in_options.workload_path) +
                    L"\"";

    if (::CreateProcessW(x86_player_path.c_str(),
                         &command_line.front  (),
                         nullptr, /* process attributes */
                         nullptr, /* thread attributes  */
                         TRUE,    /* inherit handles    */
                         CREATE_SUSPENDED,
                         nullptr, /* environment        */
                         nullptr, /* current directory  */
                        &startup_information,
                        &process_information) == FALSE)
    {
        *out_error_ptr = "The 32-bit player cannot be started.";

        goto end;
    }

    if (job_handle != nullptr)
    {
        ::AssignProcessToJobObject(job_handle,
                                   process_information.hProcess);
    }

    ::ResumeThread       (process_information.hThread);
    ::WaitForSingleObject(process_information.hProcess,
                          INFINITE);
    ::GetExitCodeProcess (process_information.hProcess,
                         &exit_code);

    *out_exit_code_ptr = static_cast<int>(exit_code);
    result             = true;

end:
    if (process_information.hThread != nullptr)
    {
        ::CloseHandle(process_information.hThread);
    }

    if (process_information.hProcess != nullptr)
    {
        ::CloseHandle(process_information.hProcess);
    }

    if (job_handle != nullptr)
    {
        ::CloseHandle(job_handle);
    }

    return result;
}
