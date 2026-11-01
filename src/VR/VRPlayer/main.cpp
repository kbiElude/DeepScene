/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * The VR player: lists the applications which can be played in VR, and the devices to play them on. It is a window, not a console
 * program; the playback itself is done by another program, which this one starts (not yet).
 */
#include "VR/AppCatalog/catalog_loader.h"
#include "VR/UIHost/imgui_window_host.h"
#include "Stereo/XR/openxr_vr_device_provider.h"
#include "VR/VRPlayer/command_line.h"
#include "VR/VRPlayer/vr_player_window.h"
#include <memory>
#include <stdlib.h>
#include <string>
#include <vector>
#include <Windows.h>

namespace VR
{
    namespace VRPlayer
    {
        std::string find_catalog();
        std::string find_runtime_directory();
        std::string get_directory(const std::string& in_path);
        std::string get_program_path();
        bool        is_file      (const std::string& in_path);
        void        show_error   (const std::string& in_message);
    }
}

std::string VR::VRPlayer::find_catalog()
{
    const std::string directory      = get_directory(get_program_path() );
    const std::string candidate_array[] =
    {
        directory + "\\apps.json",
        directory + "\\apps\\apps.json",
        directory + "\\..\\apps\\apps.json"
    };
    std::string       result;

    for (const auto& candidate : candidate_array)
    {
        if (result.empty() )
        {
            const bool is_found = is_file(candidate);

            if (is_found)
            {
                result = candidate;
            }
        }
    }

    return result;
}

std::string VR::VRPlayer::find_runtime_directory()
{
    /* What VRPlayer starts is in runtime\ of a release, and beside the program in a build folder. */
    const std::string directory      = get_directory(get_program_path() );
    const std::string candidate_array[] =
    {
        directory + "\\runtime",
        directory
    };
    std::string       result         = directory;
    bool              is_found       = false;

    for (const auto& candidate : candidate_array)
    {
        if (is_found == false)
        {
            is_found = is_file(candidate + "\\StereoPresenter.exe");

            if (is_found)
            {
                result = candidate;
            }
        }
    }

    return result;
}

std::string VR::VRPlayer::get_directory(const std::string& in_path)
{
    const size_t n_separator = in_path.find_last_of("\\/");

    return (n_separator != std::string::npos) ? in_path.substr(0,
                                                               n_separator)
                                              : std::string();
}

std::string VR::VRPlayer::get_program_path()
{
    char path_i8vec260[MAX_PATH] = {};

    ::GetModuleFileNameA(nullptr,
                         path_i8vec260,
                         MAX_PATH);

    return path_i8vec260;
}

bool VR::VRPlayer::is_file(const std::string& in_path)
{
    const DWORD attributes = ::GetFileAttributesA(in_path.c_str() );

    return (attributes != INVALID_FILE_ATTRIBUTES              &&
            (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0);
}

void VR::VRPlayer::show_error(const std::string& in_message)
{
    ::MessageBoxA(nullptr,
                  in_message.c_str(),
                  "VR Player",
                  MB_OK | MB_ICONERROR);
}

int WINAPI WinMain(HINSTANCE in_instance,
                   HINSTANCE in_previous_instance,
                   LPSTR     in_command_line,
                   int       in_show_command)
{
    std::vector<std::string>                      argument_vec;
    std::string                                   catalog_filename;
    std::string                                   error;
    VR::VRPlayer::Options                         options;
    int                                           result = EXIT_FAILURE;
    VR::VRPlayer::WindowContent                   content;
    const Stereo::XR::OpenXrVrDeviceProvider      device_provider;
    std::unique_ptr<VR::VRPlayer::VrPlayerWindow> window_ptr;

    (void) in_command_line;
    (void) in_instance;
    (void) in_previous_instance;
    (void) in_show_command;

    for (int n_argument = 1;
             n_argument < __argc;
           ++n_argument)
    {
        argument_vec.push_back(__argv[n_argument]);
    }

    if (VR::VRPlayer::CommandLine::parse(argument_vec,
                                        &options,
                                        &error) == false)
    {
        VR::VRPlayer::show_error(error + "\n\n" + VR::VRPlayer::CommandLine::get_usage_ptr() );

        goto end;
    }

    catalog_filename = (options.catalog_filename.empty() ) ? VR::VRPlayer::find_catalog()
                                                           : options.catalog_filename;

    /* A catalog which cannot be read is shown in the window, which is more use than nothing. */
    if (catalog_filename.empty() == false)
    {
        const bool is_catalog_loaded = VR::AppCatalog::CatalogLoader::load(catalog_filename,
                                                                          &content.catalog,
                                                                          &content.catalog_error);

        if (is_catalog_loaded == false)
        {
            content.catalog = VR::AppCatalog::Catalog();
        }
    }

    if (catalog_filename.empty() )
    {
        content.catalog_error = "No apps.json was found next to the program. Use --catalog to say where it is.";
    }

    if (device_provider.enumerate(&content.device_vec,
                                  &content.device_error) == false)
    {
        content.device_vec.clear();
    }

    content.launcher_directory = VR::VRPlayer::find_runtime_directory();

    VR::UIHost::ImGuiWindowHost::enable_dpi_awareness();

    window_ptr.reset(new VR::VRPlayer::VrPlayerWindow(content) );

    if (window_ptr->create(&error) == false)
    {
        VR::VRPlayer::show_error(error);

        goto end;
    }

    window_ptr->run();

    result = EXIT_SUCCESS;

end:
    return result;
}
