/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "workload_picker.h"
#include <vector>
#include <Windows.h>
#include <commdlg.h>

bool Replay::StereoReplayPlayer::WorkloadPicker::pick(std::string* out_path_ptr)
{
    /* Pairs of a description and a pattern, each ending with a 0; the list ends with one more. */
    static const wchar_t FILTER_ARRAY[] = L"Workload files (*.workload)\0*.workload\0All files (*.*)\0*.*\0";

    constexpr DWORD N_PATH_CHARACTERS = 32768;

    int                  n_bytes     = 0;
    OPENFILENAMEW        open_file   = {};
    std::vector<wchar_t> path_vec(N_PATH_CHARACTERS);
    bool                 result      = false;

    open_file.Flags       = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR | OFN_PATHMUSTEXIST;
    open_file.lStructSize = sizeof(open_file);
    open_file.lpstrFile   = path_vec.data();
    open_file.lpstrFilter = FILTER_ARRAY;
    open_file.lpstrTitle  = L"Select the workload to play back";
    open_file.nMaxFile    = static_cast<DWORD>(path_vec.size() );

    if (::GetOpenFileNameW(&open_file) == FALSE)
    {
        goto end;
    }

    /* The engine takes the path in the ANSI code page, as the command line gives it. */
    n_bytes = ::WideCharToMultiByte(CP_ACP,
                                    0,       /* flags */
                                    path_vec.data(),
                                    -1,
                                    nullptr,
                                    0,
                                    nullptr, /* default character */
                                    nullptr);

    if (n_bytes <= 0)
    {
        goto end;
    }

    out_path_ptr->resize(static_cast<size_t>(n_bytes) );

    ::WideCharToMultiByte(CP_ACP,
                          0,       /* flags */
                          path_vec.data(),
                          -1,
                          &out_path_ptr->front(),
                          n_bytes,
                          nullptr, /* default character */
                          nullptr);

    /* The count included the terminator. */
    out_path_ptr->resize(static_cast<size_t>(n_bytes) - 1);

    result = true;

end:
    return result;
}
