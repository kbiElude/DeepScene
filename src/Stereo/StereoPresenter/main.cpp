/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Shows the images which the stereo converter of an application passes on (see command_line.cpp for the arguments). It is a
 * 64-bit program whichever the architecture of the application is, since it is what will talk to the VR runtime.
 */
#include "Stereo/StereoSession/session_environment.h"
#include "Stereo/StereoTransport/frame_channel.h"
#include "Stereo/StereoTransport/host_memory_importer.h"
#include "Stereo/StereoPresenter/command_line.h"
#include "Stereo/StereoPresenter/headset_sink.h"
#include "Stereo/StereoPresenter/stereo_presenter_window.h"
#include "Stereo/StereoPresenter/presenter_log.h"
#include <memory>
#include <stdlib.h>
#include <string>
#include <vector>
#include <Windows.h>

int WINAPI WinMain(HINSTANCE in_instance,
                   HINSTANCE in_previous_instance,
                   LPSTR     in_command_line,
                   int       in_show_command)
{
    std::vector<std::string>                                  argument_vec;
    std::unique_ptr<Stereo::StereoTransport::FrameChannel>    channel_ptr;
    std::string                                               error;
    Stereo::StereoPresenter::Options                             options;
    int                                                       result = EXIT_FAILURE;
    std::unique_ptr<Stereo::StereoPresenter::StereoPresenterWindow> window_ptr;

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

    if (Stereo::StereoPresenter::CommandLine::parse(argument_vec,
                                                &options,
                                                &error) == false)
    {
        ::MessageBoxA(nullptr,
                      (error + "\n\n" + Stereo::StereoPresenter::CommandLine::get_usage_ptr() ).c_str(),
                      "VR Player",
                      MB_OK | MB_ICONERROR);

        goto end;
    }

    channel_ptr = Stereo::StereoTransport::FrameChannel::open_ptr(options.session_id,
                                                                 &error);

    if (channel_ptr == nullptr)
    {
        ::MessageBoxA(nullptr,
                      error.c_str(),
                      "VR Player",
                      MB_OK | MB_ICONERROR);

        goto end;
    }

    VR::UIHost::ImGuiWindowHost::enable_dpi_awareness();

    /* Direct3D 8 applications pass their images by the memory of the channel: it is what the importer takes. */
    window_ptr.reset(new Stereo::StereoPresenter::StereoPresenterWindow(std::unique_ptr<Stereo::StereoTransport::IFrameImporter>(new Stereo::StereoTransport::HostMemoryImporter(std::move(channel_ptr) )),
                                                                  options.title) );

    if (window_ptr->create(&error) == false)
    {
        ::MessageBoxA(nullptr,
                      error.c_str(),
                      "VR Player",
                      MB_OK | MB_ICONERROR);

        goto end;
    }

    /* The headset, if it is asked for and there is one: what cannot be done is said in the log of the session, and the window shows the
     * images as it always does.
     */
    if (options.use_headset)
    {
        const std::string log_path = Stereo::StereoSession::SessionEnvironment::get_log_path(options.session_id);
        std::string       headset_error;
        auto              headset_ptr = Stereo::StereoPresenter::HeadsetSink::create_ptr(window_ptr->get_importer_ptr(),
                                                                                     [&window_ptr]()
                                                                                     {
                                                                                         window_ptr->request_close();
                                                                                     },
                                                                                     [&log_path, &options](const std::string& in_text)
                                                                                     {
                                                                                         if (options.use_log)
                                                                                         {
                                                                                             Stereo::StereoPresenter::PresenterLog::write_line(log_path,
                                                                                                                                         in_text);
                                                                                         }
                                                                                     },
                                                                                     options.use_pacing,
                                                                                     &headset_error);

        if (headset_ptr != nullptr)
        {
            window_ptr->set_headset_sink(std::move(headset_ptr) );
        }
        else
        {
            /* Said whether a log is asked for or not: the headset that was chosen is not there to see the images. */
            Stereo::StereoPresenter::PresenterLog::write_line(log_path,
                                                        "The headset is not used: " + headset_error);
        }
    }

    window_ptr->run();

    result = EXIT_SUCCESS;

end:
    return result;
}
