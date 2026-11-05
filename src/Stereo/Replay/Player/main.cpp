/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Replays a workload file. This is the front-end of StereoReplayEngineDLL.dll: a window with the playback controls, or the
 * console (--console), which also gives the speed.
 */
#include "Analysis/APIDumpAnalysis/number_parser.h"
#include "Analysis/RecipeLoader/recipe_loader.h"
#include "Capture/APIDumpLoader/WorkloadIndex.h"
#include "Stereo/Replay/EngineDLL/entrypoints.h"
#include "command_line.h"
#include "control_window.h"
#include "frame_range_observer.h"
#include "player_launcher.h"
#include "preview_window.h"
#include "quit_handler.h"
#include "state_names.h"
#include "workload_picker.h"
#include <algorithm>
#include <iostream>
#include <memory>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <thread>
#include <vector>
#include <Windows.h>

namespace Stereo
{
    namespace Replay
    {
        namespace Player
        {
            /* Reports what happens to the replay on the console, and how steady the frames come. */
            class ConsoleObserver : public Stereo::Replay::EngineDLL::IReplayObserver
            {
            public:
                explicit ConsoleObserver(QuitHandler* in_quit_ptr);

                void on_error(const std::string&                    in_message) override;
                void on_frame_presented(const uint32_t&                       in_n_frame) override;
                void on_state_changed(const Stereo::Replay::EngineDLL::PlaybackState& in_old_state,
                                      const Stereo::Replay::EngineDLL::PlaybackState& in_new_state) override;
                void on_window_closed()                                                     override;
                void on_window_placed(const Stereo::Replay::EngineDLL::WindowPlacement& in_placement) override;

            private:
                /* The engine calls its observers one at a time, so no lock is needed here. */
                void print_frame_times(std::vector<double> in_frame_time_ms_vec);

                std::vector<double> m_frame_time_ms_vec;
                double              m_last_frame_ms;
                double              m_ms_per_tick;
                QuitHandler*        m_quit_ptr;
            };

            /* The file of the dependencies which is named like the workload: its name with the extension .deps. */
            std::string get_default_deps_filename(const std::string&                           in_workload_path);

            int  run(int                                   in_argc,
                     char*                                 in_argv[]);
            void run_console(Stereo::Replay::EngineDLL::IPlaybackController*  in_controller_ptr,
                             const std::shared_ptr<QuitHandler>&    in_quit_ptr);
        }
    }
}

Stereo::Replay::Player::ConsoleObserver::ConsoleObserver(QuitHandler* in_quit_ptr)
    :m_last_frame_ms(0.0),
     m_ms_per_tick  (0.0),
     m_quit_ptr     (in_quit_ptr)
{
    LARGE_INTEGER frequency;

    ::QueryPerformanceFrequency(&frequency);

    m_ms_per_tick = 1000.0 / static_cast<double>(frequency.QuadPart);
}

void Stereo::Replay::Player::ConsoleObserver::on_error(const std::string& in_message)
{
    fprintf(stderr,
            "[!] %s\n",
            in_message.c_str() );
}

void Stereo::Replay::Player::ConsoleObserver::on_frame_presented(const uint32_t& in_n_frame)
{
    LARGE_INTEGER ticks;

    ::QueryPerformanceCounter(&ticks);

    const double now_ms = static_cast<double>(ticks.QuadPart) * m_ms_per_tick;

    /* The time since the frame before; the first one has none. */
    if (in_n_frame != 0 &&
        m_last_frame_ms != 0.0)
    {
        m_frame_time_ms_vec.push_back(now_ms - m_last_frame_ms);
    }

    m_last_frame_ms = now_ms;
}

void Stereo::Replay::Player::ConsoleObserver::on_state_changed(const Stereo::Replay::EngineDLL::PlaybackState& in_old_state,
                                                               const Stereo::Replay::EngineDLL::PlaybackState& in_new_state)
{
    (void) in_old_state;

    fprintf(stdout,
            "[*] %s\n",
            ::Replay::ReplayPlayerSupport::StateNames::get_state_name_ptr(in_new_state) );

    /* A pause, or the end, closes a stretch of frames; the time to the next one does not count. */
    if (in_new_state != Stereo::Replay::EngineDLL::PlaybackState::PLAYING)
    {
        print_frame_times(std::move(m_frame_time_ms_vec) );

        m_frame_time_ms_vec.clear();

        m_last_frame_ms = 0.0;
    }
}

void Stereo::Replay::Player::ConsoleObserver::on_window_closed()
{
    m_quit_ptr->request();
}

void Stereo::Replay::Player::ConsoleObserver::on_window_placed(const Stereo::Replay::EngineDLL::WindowPlacement& in_placement)
{
    (void) in_placement;
}

void Stereo::Replay::Player::ConsoleObserver::print_frame_times(std::vector<double> in_frame_time_ms_vec)
{
    constexpr size_t N_PERCENT = 100;

    auto& sorted_ms_vec = in_frame_time_ms_vec;

    if (sorted_ms_vec.empty() == false)
    {
        std::sort(sorted_ms_vec.begin(),
                  sorted_ms_vec.end  () );

        fprintf(stdout,
                "[*] frame times: %zu frames, median %.2f ms, 99%% %.2f ms, worst %.2f ms\n",
                sorted_ms_vec.size(),
                sorted_ms_vec.at(sorted_ms_vec.size() / 2),
                sorted_ms_vec.at(sorted_ms_vec.size() * (N_PERCENT - 1) / N_PERCENT),
                sorted_ms_vec.back() );
    }
}

std::string Stereo::Replay::Player::get_default_deps_filename(const std::string& in_workload_path)
{
    const size_t n_slash = in_workload_path.find_last_of("\\/");
    const size_t n_dot   = in_workload_path.find_last_of('.');

    return (n_dot != std::string::npos &&
            (n_slash == std::string::npos || n_dot > n_slash) ) ? in_workload_path.substr(0,
                                                                                          n_dot) + ".deps"
                                                                : std::string();
}

int Stereo::Replay::Player::run(int   in_argc,
                                char* in_argv[])
{
    /* Used when the command line gives no distance. */
    constexpr float DEFAULT_EYE_SEPARATION = 0.1f;

    Stereo::Replay::EngineDLL::IPlaybackController* controller_ptr     = nullptr;
    constexpr uint32_t N_INDEX_READER_THREADS = 2;

    Stereo::Replay::EngineDLL::IReplayEngine*       engine_ptr         = nullptr;
    std::string                                     deps_filename;
    std::string                                     error;
    int                                             exit_code          = EXIT_FAILURE;
    Capture::APIDumpLoader::WorkloadIndex           index;
    bool                                            is_index_built     = false;
    bool                                            is_recipe_loaded   = true;
    bool                                            is_workload_picked = true;
    std::unique_ptr<ConsoleObserver>                observer_ptr;
    Options                                         options;
    std::unique_ptr<PreviewWindow>                  preview_ptr;
    std::shared_ptr<QuitHandler>                    quit_ptr(new QuitHandler() );
    std::unique_ptr<FrameRangeObserver>             range_ptr;
    Analysis::RecipeLoader::Recipe                  recipe;
    int                                             result             = EXIT_FAILURE;

    if (CommandLine::parse(in_argc,
                           in_argv,
                          &options,
                          &error) == false)
    {
        fprintf(stderr,
                "[!] %s\n%s",
                error.c_str(),
                CommandLine::get_usage_ptr() );

        goto end;
    }

    if (options.workload_path.empty() )
    {
        is_workload_picked = ::Replay::ReplayPlayerSupport::WorkloadPicker::pick(&options.workload_path);
    }

    if (is_workload_picked == false)
    {
        fprintf(stdout,
                "[*] No workload has been selected.\n");

        result = EXIT_SUCCESS;

        goto end;
    }

    /* Direct3D 7 and 8 can only be replayed by the 32-bit player. */
    if (::Replay::ReplayPlayerSupport::PlayerLauncher::is_handover_needed(options.workload_path.c_str() ) )
    {
        std::vector<std::string> argument_vec = CommandLine::get_handover_argument_vec(options);

        argument_vec.push_back(options.workload_path);

        if (::Replay::ReplayPlayerSupport::PlayerLauncher::run_x86_player("StereoReplayPlayer_x86.exe",
                                                                          argument_vec,
                                                                         &exit_code,
                                                                         &error) )
        {
            result = exit_code;
        }
        else
        {
            fprintf(stderr,
                    "[!] %s\n",
                    error.c_str() );
        }

        goto end;
    }

    if (options.recipe_filename.empty() == false)
    {
        is_recipe_loaded = Analysis::RecipeLoader::RecipeLoader::load(options.recipe_filename,
                                                                     &recipe,
                                                                     &error);
    }

    if (is_recipe_loaded == false)
    {
        fprintf(stderr,
                "[!] %s\n",
                error.c_str() );

        goto end;
    }

    engine_ptr = Stereo::Replay::EngineDLL::aiCreateReplayEngine();

    observer_ptr.reset(new ConsoleObserver(quit_ptr.get() ) );

    engine_ptr->add_replay_observer(observer_ptr.get() );

    if (engine_ptr->load(options.workload_path.c_str(),
                        &error) == false)
    {
        fprintf(stderr,
                "[!] %s\n",
                error.c_str() );

        goto end;
    }

    controller_ptr = engine_ptr->get_playback_controller_ptr();

    /* What a seek goes by: the dependencies between the frames, if the file of them is there. A file which is not of this workload is said, and not used. */
    deps_filename = (options.deps_filename.empty() ) ? get_default_deps_filename(options.workload_path)
                                                    : options.deps_filename;

    if (deps_filename.empty() == false &&
        ::GetFileAttributesA(deps_filename.c_str() ) != INVALID_FILE_ATTRIBUTES)
    {
        std::string deps_error;

        if (engine_ptr->set_dependencies_file(deps_filename.c_str(),
                                             &deps_error) )
        {
            fprintf(stdout,
                    "[*] A seek leaves out the frames which nothing after it reads, as %s says.\n",
                    deps_filename.c_str() );
        }
        else
        {
            fprintf(stderr,
                    "[!] The dependencies are not used: %s\n",
                    deps_error.c_str() );
        }
    }
    else
    if (options.deps_filename.empty() == false)
    {
        fprintf(stderr,
                "[!] The file of the dependencies cannot be found: %s\n",
                options.deps_filename.c_str() );
    }

    if (options.has_frame_range)
    {
        range_ptr.reset(new FrameRangeObserver(controller_ptr,
                                               options.first_frame,
                                               options.last_frame) );

        engine_ptr->add_replay_observer(range_ptr.get() );

        range_ptr->start();
    }

    if (options.recipe_filename.empty() == false)
    {
        Stereo::Replay::EngineDLL::FrameExecutionOptions execution_options;

        execution_options.eye  = (options.eye_mode == EyeMode::RIGHT) ? Analysis::RecipeLoader::Eye::RIGHT
                                                                      : Analysis::RecipeLoader::Eye::LEFT;
        execution_options.mode = (options.eye_mode == EyeMode::BOTH)  ? Stereo::Replay::EngineDLL::FrameExecution::TWICE
                                                                      : Stereo::Replay::EngineDLL::FrameExecution::BUFFERED_ONCE;

        /* The second execution of a frame is to begin where the first did: a frame may rely on the state which the one before it left (a render
         * target which it never sets, say), and the first execution changes it.
         */
        execution_options.restores_state = (options.eye_mode == EyeMode::BOTH);

        preview_ptr.reset(new PreviewWindow(options.eye_mode) );

        preview_ptr->set_close_func([quit_ptr]()
                                    {
                                        quit_ptr->request();
                                    });

        if (preview_ptr->create(&error) == false)
        {
            fprintf(stderr,
                    "[!] %s\n",
                    error.c_str() );

            goto end;
        }

        preview_ptr->set_frame_range(options.has_frame_range ? options.first_frame : 0,
                                     options.has_frame_range ? options.last_frame  : UINT32_MAX);

        /* The preview shows what is rendered, so the window of the application would only be in the way. */
        engine_ptr->set_windows_visible(false);

        engine_ptr->set_frame_execution (execution_options);
        engine_ptr->set_frame_image_sink(preview_ptr.get() );
        engine_ptr->set_recipe          (recipe);

        controller_ptr->set_eye_separation(options.has_eye_separation ? options.eye_separation
                                                                      : DEFAULT_EYE_SEPARATION);
    }

    if (options.is_console)
    {
        controller_ptr->play();

        /* Reading the commands blocks, so the main thread waits for the end instead; the reader is left behind. */
        std::thread input_thread(run_console,
                                 controller_ptr,
                                 quit_ptr);

        quit_ptr->wait();

        input_thread.detach();

        result = EXIT_SUCCESS;
    }
    else
    {
        ControlWindow window(controller_ptr,
                             "API Dump Player");

        if (window.create(&error) == false)
        {
            fprintf(stderr,
                    "[!] %s\n",
                    error.c_str() );

            goto end;
        }

        if (options.recipe_filename.empty() == false)
        {
            window.show_eye_separation();
        }

        /* The slider that seeks needs the number of frames: the index of the workload says (it is made the first time, and kept next to it). */
        if (Capture::APIDumpLoader::WorkloadIndex::load_or_build(options.workload_path.c_str(),
                                                                 N_INDEX_READER_THREADS,
                                                                &index,
                                                                &is_index_built,
                                                                &error) )
        {
            window.set_n_frames(index.get_n_frames() );
        }

        quit_ptr->set_target([&window]()
                             {
                                 window.request_close();
                             });

        /* The window is up before the replay makes its own, so that it can be put below it. */
        engine_ptr->add_replay_observer(&window);

        controller_ptr->play();

        window.run();

        quit_ptr->set_target(nullptr);

        engine_ptr->remove_replay_observer(&window);

        result = EXIT_SUCCESS;
    }

end:
    if (engine_ptr != nullptr)
    {
        if (range_ptr != nullptr)
        {
            engine_ptr->remove_replay_observer(range_ptr.get() );
        }

        engine_ptr->remove_replay_observer(observer_ptr.get() );

        Stereo::Replay::EngineDLL::aiDestroyReplayEngine(engine_ptr);
    }

    return result;
}

void Stereo::Replay::Player::run_console(Stereo::Replay::EngineDLL::IPlaybackController* in_controller_ptr,
                                         const std::shared_ptr<QuitHandler>&             in_quit_ptr)
{
    for (std::string command; std::getline(std::cin,
                                           command); )
    {
        /* The player has ended already, and the controller may be gone. */
        if (in_quit_ptr->is_requested() )
        {
            break;
        }

        if (command == "q")
        {
            break;
        }

        if (command == "p")
        {
            const auto status = in_controller_ptr->get_status();

            if (status.state == Stereo::Replay::EngineDLL::PlaybackState::PLAYING)
            {
                in_controller_ptr->pause();
            }
            else
            {
                in_controller_ptr->play();
            }
        }

        if (command == "r")
        {
            in_controller_ptr->reset();
        }

        if (command == "s")
        {
            const auto status = in_controller_ptr->get_status();

            fprintf(stdout,
                    "[*] %s: %llu calls, %u frames\n",
                    ::Replay::ReplayPlayerSupport::StateNames::get_state_name_ptr(status.state),
                    static_cast<unsigned long long>(status.n_calls_executed),
                    status.n_frames_presented);
        }

        if (command.compare(0,
                            2,
                            "e ") == 0)
        {
            double separation = 0.0;

            if (Analysis::APIDumpAnalysis::NumberParser::parse_double(command.substr(2),
                                                                      &separation) )
            {
                in_controller_ptr->set_eye_separation(static_cast<float>(separation) );
            }
            else
            {
                fprintf(stderr,
                        "[!] e needs a distance which is not negative, such as 0.1\n");
            }
        }

        if (command.compare(0,
                            2,
                            "g ") == 0)
        {
            uint64_t frame = 0;

            if (Analysis::APIDumpAnalysis::NumberParser::parse_unsigned(command.substr(2),
                                                                        UINT32_MAX,
                                                                       &frame) )
            {
                in_controller_ptr->play_to_frame(static_cast<uint32_t>(frame) );
            }
            else
            {
                fprintf(stderr,
                        "[!] g needs a frame number, such as 300\n");
            }
        }

        if (command.compare(0,
                            2,
                            "x ") == 0)
        {
            double speed = 0.0;

            if (Analysis::APIDumpAnalysis::NumberParser::parse_double(command.substr(2),
                                                                      &speed) )
            {
                in_controller_ptr->set_speed(speed);
            }
            else
            {
                fprintf(stderr,
                        "[!] x needs a speed such as 1 or 0.5, and 0 for no limit\n");
            }
        }
    }
}

int main(int   in_argc,
         char* in_argv[])
{
    return Stereo::Replay::Player::run(in_argc,
                                       in_argv);
}
