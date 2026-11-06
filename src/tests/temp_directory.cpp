/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "temp_directory.h"
#include "vr/app_installer/file_system.h"
#include <Windows.h>
#include <atomic>

Tests::TempDirectory::TempDirectory()
{
    static std::atomic<int> n_directories_made(0);

    std::string error;
    wchar_t     temp_path_i16vec260[MAX_PATH] = {};

    ::GetTempPathW(MAX_PATH,
                   temp_path_i16vec260);

    m_path = VR::AppInstaller::FileSystem::to_utf8(temp_path_i16vec260) + "DemoVRPlayerTests_" + std::to_string(::GetCurrentProcessId() ) + "_" +
             std::to_string(n_directories_made++);

    VR::AppInstaller::FileSystem::create_directories(m_path,
                                                     &error);
}

Tests::TempDirectory::~TempDirectory()
{
    VR::AppInstaller::FileSystem::remove_tree(m_path);
}

const std::string& Tests::TempDirectory::get_path() const
{
    return m_path;
}
