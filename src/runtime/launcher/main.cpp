/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Starts an application with the stereo converter in it (see command_line.cpp for the arguments). The converter is a DLL of the
 * architecture of the application, so there is a launcher of each: it is the launcher which Detours can inject from.
 */
#include "analysis/recipe/recipe_loader.h"
#include "Capture/InjectionLauncher/launcher.h"
#include "stereo/session/session_environment.h"
#include "command_line.h"
#include "log_summary.h"
#include <stdio.h>
#include <stdlib.h>
#include <fstream>
#include <string>
#include <vector>
#include <Windows.h>

namespace Stereo
{
    namespace Launcher
    {
        std::string              get_converter_failure_reason(const std::string&              in_session_id);
        std::string              get_own_directory           ();
        bool                     is_file                     (const std::string&              in_path);
        std::vector<std::string> read_log_lines              (const std::string&              in_session_id);
        int                      run                         (const std::vector<std::string>& in_argument_vec);
    }
}

std::string Stereo::Launcher::get_converter_failure_reason(const std::string& in_session_id)
{
    const std::string marker = "The application cannot be converted:";
    std::string       line;
    std::ifstream log(Stereo::Session::SessionEnvironment::get_log_path(in_session_id));
    std::string       result = "The converter stopped the application; its log says why.";

    /* The last of the lines it wrote, without what it adds to each (the process, the program, the recipe). */
    while (std::getline(log,
                        line) )
    {
        const auto marker_position = line.find(marker);

        if (marker_position != std::string::npos)
        {
            const auto details_position = line.rfind(" (process ");

            result = line.substr(marker_position,
                                 (details_position != std::string::npos && details_position > marker_position) ? details_position - marker_position
                                                                                                               : std::string::npos);
        }
    }

    return result;
}

std::string Stereo::Launcher::get_own_directory()
{
    char path_i8vec260[MAX_PATH] = {};

    ::GetModuleFileNameA(nullptr,
                         path_i8vec260,
                         MAX_PATH);

    const std::string path                  = path_i8vec260;
    const size_t      n_last_separator_char = path.find_last_of('\\');

    return path.substr(0,
                       n_last_separator_char);
}

bool Stereo::Launcher::is_file(const std::string& in_path)
{
    const DWORD attributes = ::GetFileAttributesA(in_path.c_str() );

    return (attributes != INVALID_FILE_ATTRIBUTES              &&
            (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0);
}

std::vector<std::string> Stereo::Launcher::read_log_lines(const std::string& in_session_id)
{
    std::string              line;
    std::ifstream log(Stereo::Session::SessionEnvironment::get_log_path(in_session_id));
    std::vector<std::string> result_vec;

    while (std::getline(log,
                        line) )
    {
        result_vec.push_back(line);
    }

    return result_vec;
}

int Stereo::Launcher::run(const std::vector<std::string>& in_argument_vec)
{
    Capture::InjectionLauncher::TargetArchitecture architecture = Capture::InjectionLauncher::TargetArchitecture::UNKNOWN;
    std::string                                    error;
    Capture::InjectionLauncher::LaunchRequest      request;
    Options                                        options;
    PROCESS_INFORMATION                            process_info = {};
    Analysis::Recipe::Recipe                       recipe;
    ExitCode                                       result       = ExitCode::LAUNCH_FAILED;
    Stereo::Session::SessionInfo                   session_info;

    if (CommandLine::parse(in_argument_vec,
                          &options,
                          &error) == false)
    {
        fprintf(stderr,
                "[!] %s\n%s",
                error.c_str(),
                CommandLine::get_usage_ptr() );

        result = ExitCode::BAD_ARGUMENTS;

        goto end;
    }

    if (is_file(options.executable_path) == false)
    {
        fprintf(stderr,
                "[!] The application [%s] cannot be found.\n",
                options.executable_path.c_str() );

        goto end;
    }

    if (Capture::InjectionLauncher::Launcher::get_target_architecture(options.executable_path,
                                                                     &architecture,
                                                                     &error) == false)
    {
        fprintf(stderr,
                "[!] Could not determine the architecture of [%s]: %s\n",
                options.executable_path.c_str(),
                error.c_str() );

        goto end;
    }

    if (architecture != Capture::InjectionLauncher::Launcher::get_own_architecture() )
    {
        const bool is_x86 = (architecture == Capture::InjectionLauncher::TargetArchitecture::X86);

        fprintf(stderr,
                "[!] [%s] is an %s application, and this is the %s launcher. Use StereoLauncher_%s.exe instead.\n",
                options.executable_path.c_str(),
                (is_x86) ? "x86" : "x64",
                (is_x86) ? "x64" : "x86",
                (is_x86) ? "x86" : "x64");

        goto end;
    }

    request.dll_path = get_own_directory() + ((architecture == Capture::InjectionLauncher::TargetArchitecture::X86) ? "\\StereoConverterDLL_x86.dll"
                                                                                                            : "\\StereoConverterDLL_x64.dll");

    if (is_file(request.dll_path) == false)
    {
        fprintf(stderr,
                "[!] The converter [%s] cannot be found.\n",
                request.dll_path.c_str() );

        goto end;
    }

    /* A recipe which cannot be used is better found here than after the application is up. */
    if (Analysis::Recipe::RecipeLoader::load(options.recipe_path,
                                             &recipe,
                                             &error) == false)
    {
        fprintf(stderr,
                "[!] The recipe [%s] cannot be used: %s\n",
                options.recipe_path.c_str(),
                error.c_str() );

        goto end;
    }

    session_info.is_logging  = options.is_logging;
    session_info.recipe_path = options.recipe_path;
    session_info.session_id  = options.session_id;

    if (Stereo::Session::SessionEnvironment::write(session_info) == false)
    {
        fprintf(stderr,
                "[!] The playback cannot be handed over to the converter.\n");

        goto end;
    }

    request.arguments         = options.arguments;
    request.executable_path   = options.executable_path;
    request.working_directory = options.working_directory;

    if (Capture::InjectionLauncher::Launcher::launch(request,
                                                    &process_info,
                                                    &error) == false)
    {
        fprintf(stderr,
                "[!] %s\n",
                error.c_str() );

        goto end;
    }

    fprintf(stdout,
            "[*] Started [%s] (process %lu).\n",
            options.executable_path.c_str(),
            process_info.dwProcessId);
    fflush(stdout);

    if (options.should_wait)
    {
        DWORD exit_code = 0;

        ::WaitForSingleObject(process_info.hProcess,
                              INFINITE);
        ::GetExitCodeProcess (process_info.hProcess,
                             &exit_code);

        fprintf(stdout,
                "[*] The application exited with code %lu.\n",
                exit_code);

        /* The converter stops an application which it cannot convert, and says why in its log. */
        if (exit_code == Stereo::Session::SessionEnvironment::get_converter_failed_exit_code() )
        {
            const std::string failure_reason = get_converter_failure_reason(options.session_id);

            fprintf(stdout,
                    "[!] %s\n",
                    failure_reason.c_str() );

            result = ExitCode::CONVERTER_FAILED;
        }

        /* What the converter said of the run, in a few lines. */
        if (options.is_logging)
        {
            for (const auto& report_line : LogSummarizer::make_report_lines(LogSummarizer::summarize(read_log_lines(options.session_id) ) ) )
            {
                fprintf(stdout,
                        "%s\n",
                        report_line.c_str() );
            }
        }
    }

    ::CloseHandle(process_info.hProcess);
    ::CloseHandle(process_info.hThread);

    if (result != ExitCode::CONVERTER_FAILED)
    {
        result = ExitCode::SUCCESS;
    }

end:
    return static_cast<int>(result);
}

int main(int   in_argc,
         char* in_argv[])
{
    std::vector<std::string> argument_vec;

    for (int n_argument = 1;
             n_argument < in_argc;
           ++n_argument)
    {
        argument_vec.push_back(in_argv[n_argument]);
    }

    return Stereo::Launcher::run(argument_vec);
}
